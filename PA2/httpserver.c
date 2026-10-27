/**
 * @File httpserver.c
 *
 * This file contains the main function for the HTTP server.
 *
 * @author Elder Ivan Argueta
 */

#include <ctype.h>

#include "iowrapper.h"
#include "listener_socket.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

// Items that are sent in a request from client
typedef struct Request_t {
  size_t content_length;
  int has_content_length;
  int header_start;
  int header_end;
  int body_start;
  int body_end;
  int bytes_read;
  char method[9];
  char version[9];
  char uri[65];
} Request;

void send_response(int connfd, int code) {
  const char *status_phrase;
  const char *message_body;
  switch (code) {
  case 200:
    message_body = "OK\n";
    status_phrase = "OK";
    break;
  case 201:
    message_body = "Created\n";
    status_phrase = "Created";
    break;
  case 400:
    message_body = "Bad Request\n";
    status_phrase = "Bad Request";
    break;
  case 403:
    message_body = "Forbidden\n";
    status_phrase = "Forbidden";
    break;
  case 404:
    message_body = "Not Found\n";
    status_phrase = "Not Found";
    break;
  case 500:
    message_body = "Internal Server Error\n";
    status_phrase = "Internal Server Error";
    break;
  case 501:
    message_body = "Not Implemented\n";
    status_phrase = "Not Implemented";
    break;
  case 505:
    message_body = "Version Not Supported\n";
    status_phrase = "Version Not Supported";
    break;
  default:
    code = 500;
    message_body = "Internal Server Error\n";
    status_phrase = "Internal Server Error";
    break;
  }

  char response[256];

  int length = snprintf(response, sizeof(response),
                        "HTTP/1.1 %d %s\r\nContent-Length: %zu\r\n\r\n%s", code,
                        status_phrase, strlen(message_body), message_body);

  if (length < 0)
    return;

  if ((size_t)length >= sizeof(response))
    return;

  write_n_bytes(connfd, response, length);
}

int read_header(char buffer[BUFFER_SIZE], Request *request, int connfd,
                int valid_buffer) {

  int read_res = 0;
  int continue_reading = 1;

  while (continue_reading) {
    if (request->bytes_read >= BUFFER_SIZE) {
      continue_reading = 0;
      send_response(connfd, 400);
      continue;
    }

    // connfd is where to read from, buffer + bytes_read is where to place bytes
    // at,
    read_res = read(connfd, buffer + request->bytes_read,
                    BUFFER_SIZE - request->bytes_read);
    // read only remaining bytes to buffer + bytes_read from connfd

    if (read_res == -1) {
      if (errno == EINTR) {
        continue;
      }
      send_response(connfd, 500);
      break;
    }

    if (read_res == 0) {
      continue_reading = 0;
      send_response(connfd, 400);
      continue;
    }

    request->bytes_read +=
        read_res; // add to the amount of bytes read to the buffer

    if (request->bytes_read >= 4) {
      for (int i = 0; i <= request->bytes_read - 4; i++) {
        if (buffer[i] == '\r' && buffer[i + 1] == '\n' &&
            buffer[i + 2] == '\r' && buffer[i + 3] == '\n') {
          request->header_end = i + 4;

          if (request->header_end > 2048) {
            valid_buffer = 0;
            continue_reading = 0;
            send_response(connfd, 400);
            break;
          }

          valid_buffer = 1;
          continue_reading = 0;
          break;
        }
      }
    }

    if (request->header_end == 0 && request->bytes_read >= 2048) {
      valid_buffer = 0;
      continue_reading = 0;
      send_response(connfd, 400);
      break;
    }
  }

  return valid_buffer;
}

int read_method(char buffer[BUFFER_SIZE], Request *request, int connfd,
                int valid_buffer, int valid_method, int *uri_start) {
  // if we have made a valid header
  int error_sent = 0;
  if (valid_buffer) {
    // loop until end of valid header
    for (int i = 0; i < request->header_end; i++) {
      if (i == 0) {
        // if first char is a space
        if (buffer[i] == ' ') {
          // send bad request error
          send_response(connfd, 400);
          valid_method = 0;
          error_sent = 1;
          break;
        }
      }

      // check if we've seen a space indicating end of method
      if (buffer[i] == ' ') {
        request->method[i] = '\0';
        valid_method = 1;
        *uri_start = i + 1;
        break;
      }

      // if i is greater than method buffer
      // send 400 error
      if (i == 8) {
        send_response(connfd, 400);
        valid_method = 0;
        error_sent = 1;
        break;
      }

      // only accept valid chars
      if ((buffer[i] >= 'A' && buffer[i] <= 'Z') ||
          (buffer[i] >= 'a' && buffer[i] <= 'z')) {
        request->method[i] = buffer[i];
      } else {
        // send bad request error
        send_response(connfd, 400);
        valid_method = 0;
        error_sent = 1;
        break;
      }
    }
  }

  if (!valid_method && !error_sent) {
    send_response(connfd, 400);
  }

  return valid_method;
}

int read_uri(char buffer[BUFFER_SIZE], Request *request, int connfd,
             int valid_method, int valid_uri, const int *uri_start,
             int *version_start) {
  int uri_index = 0;
  int error_sent = 0;

  // if we have made a valid method
  if (valid_method) {
    if (buffer[*uri_start] == '/') {
      // check if char at uri_start is a /
      request->uri[uri_index++] = '/';
      for (int i = *uri_start + 1; i < request->header_end; i++) {
        // check for space char to end uri
        if (buffer[i] == ' ') {
          if (uri_index < 2) {
            send_response(connfd, 400);
            valid_uri = 0;
            error_sent = 1;
            break;
          }
          request->uri[uri_index] = '\0';
          valid_uri = 1;
          *version_start = i + 1;
          break;
        }

        // if i reaches size of uri, and we haven't made uri
        // send 400 error
        if (uri_index >= 64) {
          send_response(connfd, 400);
          valid_uri = 0;
          error_sent = 1;
          break;
        }

        // only accept valid characters
        if ((buffer[i] >= 'A' && buffer[i] <= 'Z') ||
            (buffer[i] >= 'a' && buffer[i] <= 'z') || buffer[i] == '.' ||
            buffer[i] == '-' || isdigit((unsigned char)buffer[i])) {
          request->uri[uri_index++] = buffer[i];
        } else {
          // else send bad request error
          send_response(connfd, 400);
          valid_uri = 0;
          error_sent = 1;
          break;
        }
      }
    } else {
      send_response(connfd, 400);
      valid_uri = 0;
      error_sent = 1;
    }
  }

  if (!valid_uri && !error_sent) {
    send_response(connfd, 400);
  }

  return valid_uri;
}

int read_version(char buffer[BUFFER_SIZE], Request *request, int connfd,
                 int valid_uri, int valid_version_length, int *version_start) {
  int version_index = 0;
  int error_sent = 0;
  // check if we have a valid uri
  if (valid_uri) {
    // loop starting from version start to end of header
    for (int i = *version_start; i < request->header_end; i++) {
      // check i + 1 is still less than header_end AND if buffer at i is \r AND
      // if buffer at i + 1 is \n
      if (i + 1 < request->header_end && buffer[i] == '\r' &&
          buffer[i + 1] == '\n') {

        // check if index is not correct version size
        if (version_index != 8) {
          // send error code
          send_response(connfd, 400);
          error_sent = 1;
          break;
        }
        // add null terminator to version
        request->version[version_index] = '\0';
        request->header_start = i + 2;
        valid_version_length = 1; // found valid version
        break;
      }

      // if valid index is greater than or equal to 8
      if (version_index >= 8) {
        // send 400 error code
        send_response(connfd, 400);
        error_sent = 1;
        break;
      }
      // finally add to version array
      request->version[version_index++] = buffer[i];
    }

    if (!valid_version_length && !error_sent) {
      send_response(connfd, 400);
    }
  }

  return valid_version_length;
}

int check_version(Request *request, int connfd, int valid_version_length,
                  int valid_version) {
  if (valid_version_length) {
    // check if request.version has valid HTTP/ and digit.digit
    if (strncmp(request->version, "HTTP/", 5) != 0 ||
        !isdigit((unsigned char)request->version[5]) ||
        request->version[6] != '.' ||
        !isdigit((unsigned char)request->version[7])) {
      send_response(connfd, 400);
      valid_version = 0;
    } else {
      // check if version has 1 for both digit values in version
      if (request->version[5] != '1' || request->version[7] != '1') {
        // if not send 505 error code
        send_response(connfd, 505);
        valid_version = 0;
      } else {
        // else we have a valid version
        valid_version = 1;
      }
    }
  }

  return valid_version;
}

int read_content_length(char buffer[BUFFER_SIZE], Request *request,
                        int connfd) {
  int valid_content = 0;
  int error_sent = 0;

  int line_start = request->header_start;
  int i = line_start;

  char value[129];
  int value_index = 0;

  // while index is less than header_end value
  while (i < request->header_end) {
    int content_length_found = 0;
    value_index = 0;
    // check if i + 1 is valid location and if buffer at i and i + 1 is \r\n
    if (i + 1 < request->header_end && buffer[i] == '\r' &&
        buffer[i + 1] == '\n') {
      int line_length = i - line_start;

      if (line_length == 0) {
        valid_content = 1;
        break;
      }

      if (line_length > 0) {
        int colon_index = -1;

        // loop from line_start until \r where i is
        for (int j = line_start; j < i; j++) {
          // if we find a colon break out of for loop
          if (buffer[j] == ':') {
            colon_index = j;
            break;
          }
        }

        // no colon found send error
        if (colon_index == -1) {
          send_response(connfd, 400);
          error_sent = 1;
          break;
        }

        int key_length = colon_index - line_start;

        if (key_length >= 1 && key_length <= 128) {
          for (int k = line_start; k < colon_index; k++) {
            if ((buffer[k] >= 'a' && buffer[k] <= 'z') ||
                (buffer[k] >= 'A' && buffer[k] <= 'Z') ||
                (buffer[k] >= '0' && buffer[k] <= '9') || buffer[k] == '.' ||
                buffer[k] == '-') {
            } else {
              send_response(connfd, 400);
              error_sent = 1;
              return valid_content;
            }
          }
        } else {
          send_response(connfd, 400);
          error_sent = 1;
          return valid_content;
        }

        if (key_length == 14) {
          if (strncmp(&buffer[line_start], "Content-Length", key_length) == 0)
            content_length_found = 1;
        } else {
          content_length_found = 0;
        }

        // check if there's a space after colon
        if (buffer[colon_index + 1] == ' ') {
          // start 2 places after colon to get first value character
          for (int j = colon_index + 2; j < i; j++) {
            // if value_index is equal to value buffer length
            // throw error
            if (value_index == 128) {
              send_response(connfd, 400);
              error_sent = 1;
              return valid_content;
            }
            // check if value is between 32 and 126  ascii character
            if (buffer[j] >= 32 && buffer[j] <= 126) {
              value[value_index++] = buffer[j];
            } else { // else send error
              send_response(connfd, 400);
              error_sent = 1;
              return valid_content;
            }
          }

          // if we didn't move value_index send error
          if (value_index == 0) {
            send_response(connfd, 400);
            error_sent = 1;
            break;
          }

          // add null terminator and reset value_index in case of another loop.
          value[value_index] = '\0';

          // Check if value's characters are valid numbers
          if (content_length_found) {
            for (int v = 0; v < value_index; v++) {
              if (!isdigit(value[v])) {
                send_response(connfd, 400);
                error_sent = 1;
                return valid_content;
              }
            }

            // convert to size_t number
            char *end;
            errno = 0;
            size_t length = strtoul(value, &end, 10);

            // overflow error check
            if (errno == ERANGE) {
              send_response(connfd, 400);
              error_sent = 1;
              return valid_content;
            }

            // end ptr isn't pointing to null terminator
            if (*end != '\0') {
              send_response(connfd, 400);
              return valid_content;
            }

            request->content_length = length;
            request->has_content_length = 1;
          }

        } else {
          send_response(connfd, 400);
          error_sent = 1;
          break;
        }
      }

      // skip over current \r\n to next content header
      line_start = i + 2;
      i = line_start;
    } else {
      i++;
    }
  }

  if (!valid_content && !error_sent) {
    send_response(connfd, 400);
  }
  return valid_content;
}

int handle_get(const Request *request, int connfd) {
  struct stat sb;
  int get_ok = 0; // return 1 for valid get handling else 0 for failure

  int open_res = open(request->uri + 1, O_RDONLY);

  // check what type of error was sent to errno
  if (open_res < 0) {
    if (errno == ENOENT) {
      send_response(connfd, 404);
      return get_ok;
    } else if (errno == EACCES) {
      send_response(connfd, 403);
      return get_ok;
    } else {
      send_response(connfd, 500);
      return get_ok;
    }
  }

  // check if stat was successful
  if (fstat(open_res, &sb) < 0) {
    send_response(connfd, 500);
    close(open_res);
    return get_ok;
  }

  // check if we have the right permission for file
  if (!S_ISREG(sb.st_mode)) {
    send_response(connfd, 403);
    close(open_res);
    return get_ok;
  }

  // check if num bytes in file is valid
  if (sb.st_size < 0) {
    send_response(connfd, 500);
    close(open_res);
    return get_ok;
  }

  size_t file_size = (size_t)sb.st_size;

  // send response
  char response[256];
  const char *ok = "OK";
  const int code = 200;
  int length = snprintf(response, sizeof(response),
                        "HTTP/1.1 %d %s\r\nContent-Length: %zu\r\n\r\n", code,
                        ok, file_size);

  if (length < 0 || (size_t)length >= sizeof(response)) {
    send_response(connfd, 500);
    close(open_res);
    return get_ok;
  }

  ssize_t write_n_bytes_value = write_n_bytes(connfd, response, length);

  if (write_n_bytes_value < 0 || write_n_bytes_value != length) {
    close(open_res);
    return get_ok;
  }

  ssize_t pass_n_bytes_value = pass_n_bytes(open_res, connfd, file_size);

  if (pass_n_bytes_value < 0 || (size_t)pass_n_bytes_value != file_size) {
    close(open_res);
    return get_ok;
  }

  get_ok = 1;
  close(open_res);

  return get_ok;
}

int handle_put(char buffer[BUFFER_SIZE], Request *request, int connfd) {
  int put_ok = 0;

  struct stat sb;

  int file_exists = 0;

  // if not content length is found send error
  if (!request->has_content_length) {
    send_response(connfd, 400);
    return put_ok;
  }

  // find out if file exists
  int stat_value = stat(request->uri + 1, &sb);

  if (stat_value == 0) {
    if (!S_ISREG(sb.st_mode)) {
      send_response(connfd, 403);
      return put_ok;
    }
    file_exists = 1;
  }

  if (stat_value < 0) {
    if (errno == ENOENT) {
      file_exists = 0;
    } else if (errno == EACCES) {
      send_response(connfd, 403);
      return put_ok;
    } else {
      send_response(connfd, 500);
      return put_ok;
    }
  }

  int open_res = open(request->uri + 1, O_CREAT | O_WRONLY | O_TRUNC, 0644);

  if (open_res < 0) {
    if (errno == EACCES) {
      send_response(connfd, 403);
      return put_ok;
    } else if (errno == EPERM) {
      send_response(connfd, 403);
      return put_ok;
    } else if (errno == EISDIR) {
      send_response(connfd, 403);
      return put_ok;
    } else {
      send_response(connfd, 500);
      return put_ok;
    }
  }

  size_t buffered_bytes = request->bytes_read - request->header_end;

  // check if buffered bytes is more than requests content length
  if (buffered_bytes > request->content_length) {
    buffered_bytes =
        request->content_length; // cap buffered_bytes to content_length
  }

  // write bytes out from buffer + header_end until buffered_bytes
  ssize_t write_n_bytes_value =
      write_n_bytes(open_res, buffer + request->header_end, buffered_bytes);

  if (write_n_bytes_value < 0 ||
      (size_t)write_n_bytes_value != buffered_bytes) {
    send_response(connfd, 500);
    close(open_res);
    return put_ok;
  }

  size_t remaining_bytes = request->content_length - buffered_bytes;

  if (remaining_bytes > 0) {
    ssize_t pass_n_bytes_value =
        pass_n_bytes(connfd, open_res, remaining_bytes);

    if (pass_n_bytes_value < 0) {
      close(open_res);
      send_response(connfd, 500);
      return put_ok;
    }

    if ((size_t)pass_n_bytes_value != remaining_bytes) {
      close(open_res);
      send_response(connfd, 400);
      return put_ok;
    }
  }

  if (close(open_res) < 0) {
    send_response(connfd, 500);
    return put_ok;
  }

  if (file_exists) {
    send_response(connfd, 200);
  } else {
    send_response(connfd, 201);
  }

  put_ok = 1;
  return put_ok;
}

void handle_connection(int connfd) {
  Request request;
  request.bytes_read = 0;
  request.content_length = 0;
  request.has_content_length = 0;
  request.header_start = 0;
  request.header_end = 0;

  int valid_buffer = 0;
  int valid_method = 0;
  int valid_uri = 0;
  int valid_version = 0;
  int valid_content_length = 0;

  int uri_start = 0;

  int version_start = 0;
  int valid_version_length = 0;

  char buffer[BUFFER_SIZE];

  valid_buffer = read_header(buffer, &request, connfd, valid_buffer);

  if (valid_buffer) {
    valid_method = read_method(buffer, &request, connfd, valid_buffer,
                               valid_method, &uri_start);
  } else {
    close(connfd);
    return;
  }

  if (valid_method) {
    valid_uri = read_uri(buffer, &request, connfd, valid_method, valid_uri,
                         &uri_start, &version_start);
  } else {
    close(connfd);
    return;
  }

  if (valid_uri) {
    valid_version_length = read_version(buffer, &request, connfd, valid_uri,
                                        valid_version_length, &version_start);
  } else {
    close(connfd);
    return;
  }

  if (valid_version_length) {
    valid_version =
        check_version(&request, connfd, valid_version_length, valid_version);
  } else {
    close(connfd);
    return;
  }

  if (valid_version) {
    valid_content_length = read_content_length(buffer, &request, connfd);
  } else {
    close(connfd);
    return;
  }

  if (!valid_content_length) {
    close(connfd);
    return;
  }

  if (strcmp(request.method, "GET") == 0) {
    handle_get(&request, connfd);
  } else if (strcmp(request.method, "PUT") == 0) {
    handle_put(buffer, &request, connfd);
  } else {
    send_response(connfd, 501);
  }

  close(connfd);
  return;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    write(STDERR_FILENO, "Invalid Port\n", 13);
    return EXIT_FAILURE;
  }

  if (argv[1][0] == '-') {
    write(STDERR_FILENO, "Invalid Port\n", 13);
    return EXIT_FAILURE;
  }

  char *endptr = NULL;
  errno = 0;
  // Pointer to first passed value. Pointer to where last read ends, base to
  // turn number into
  size_t port = (size_t)strtoull(argv[1], &endptr, 10);

  // check port overflow
  if (errno == ERANGE) {
    write(STDERR_FILENO, "Invalid Port\n", 13);
    return EXIT_FAILURE;
  }
  // check if left over characters in port
  if (endptr == argv[1]) {
    write(STDERR_FILENO, "Invalid Port\n", 13);
    return EXIT_FAILURE;
  }

  if (*endptr != '\0') {
    write(STDERR_FILENO, "Invalid Port\n", 13);
    return EXIT_FAILURE;
  }

  if (port < 1 || port > 65535) {
    write(STDERR_FILENO, "Invalid Port\n", 13);
    return EXIT_FAILURE;
  }

  /* Add error checking for the port number */

  signal(SIGPIPE, SIG_IGN);
  Listener_Socket_t *sock;
  sock = ls_new(port);

  // Passed port is already in use
  if (sock == NULL) {
    write(STDERR_FILENO, "Invalid Port\n", 13);
    return EXIT_FAILURE;
  }

  while (1) {
    int connfd = ls_accept(sock);
    if (connfd < 0) {
      continue;
    }
    handle_connection(connfd);
  }

  return EXIT_SUCCESS;
}
