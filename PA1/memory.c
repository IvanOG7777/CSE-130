#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>

const char *invalid = "Invalid Command\n";
const char *operation = "Operation Failed\n";
void invalid_command(void) {
    write(STDERR_FILENO, invalid, strlen(invalid));
    exit(1);
}

void error(void) {
    write(STDERR_FILENO, operation, strlen(operation));
    exit(1);
}

int main(void) {
    char buffer[4096];
    char command[4];
    char file_name[PATH_MAX];
    int i = 0;
    int file_name_index = 0;
    int command_index = 0;

    ssize_t read_res = read(STDIN_FILENO, buffer, sizeof(buffer)); // reads init 256 commands
    if (read_res < 0) {
        // if bad read throw error
        error();
    }

    // reads until get or set command
    int continue_command_reading = 1;
    while (continue_command_reading) {
        while (i < read_res) {
            if (buffer[i] == '\n') {
                continue_command_reading = 0;
                i++;
                break;
            }

            if (command_index >= 3) {
                invalid_command();
            }

            command[command_index] = buffer[i];
            command_index++;
            i++;
        }

        if (continue_command_reading == 0) {
            break;
        }
        // read next chunk
        read_res = read(STDIN_FILENO, buffer, sizeof(buffer));

        // check for errors from read()
        if (read_res < 0) {
            error();
        }
        if (read_res == 0) {
            invalid_command();
        }

        // reset i to keep reading
        i = 0;
    }

    command[command_index] = '\0';

    int continue_file_name_reading = 1; // flag for while to keep reading chunks
    while (continue_file_name_reading) { // while true

        // while i < amount of bytes read from read
        while (i < read_res) {
            // reached end of file name
            if (buffer[i] == '\n') {
                continue_file_name_reading = 0;
                i++;
                break;
            }

            // if file index reached max file buffer size
            if (file_name_index >= PATH_MAX - 1) {
                invalid_command(); // throw error
            }

            // set value at i
            file_name[file_name_index] = buffer[i];
            file_name_index++;
            i++;
        }

        if (continue_file_name_reading == 0) {
            break;
        }
        // read next chunk
        read_res = read(STDIN_FILENO, buffer, sizeof(buffer));

        // check for errors from read()
        if (read_res < 0) {
            error();
        }
        if (read_res == 0) {
            invalid_command();
        }

        // reset i to keep reading
        i = 0;
    }

    file_name[file_name_index] = '\0';


    if (strcmp(command, "get") == 0) {

        if (i < read_res) {
            invalid_command();
        }

        // if extra bytes in read after file reading throw error
        read_res = read(STDIN_FILENO, buffer, sizeof(buffer));

        if (read_res < 0) {
            error();
        }

        if (read_res > 0) {
            invalid_command();
        }


        int get_fd = open(file_name, O_RDONLY);

        if (get_fd < 0) {
            invalid_command();
        }

        ssize_t bytes_read = 0;

        // keep reading bytes from file
        while (1) {
            bytes_read = read(get_fd, buffer, sizeof(buffer));

            // When we cant read anymore bytes
            if (bytes_read == 0) {
                ssize_t close_res = close(get_fd); // close file
                // if bad close throw error
                if (close_res < 0) {
                    error();
                }
                break; // break out of while loop
            }

            // if bad read from file
            if (bytes_read < 0) {
                // close file and throw error
                close(get_fd);

                if (errno == EISDIR) {
                    invalid_command();
                }

                error();
            }

            ssize_t bytes_written = 0;

            while (bytes_written < bytes_read) {
                // write out, start at pointer of buffer + bytes_written on each loop, write only bytes_read - bytes_written
                ssize_t write_res = write(STDOUT_FILENO, buffer + bytes_written, bytes_read - bytes_written);
                // return -1 or num of bytes written

                // if bad write close file and throw error
                if (write_res <= 0) {
                    close(get_fd);
                    error();
                }

                bytes_written += write_res;
            }
        }
        //  up to set both command and file name are already read in
    } else if (strcmp(command, "set") == 0) {


        char bytes_requested[PATH_MAX];
        int bytes_requested_index = 0;

        int continue_reading_requested_bytes = 1;
        //continue reading the bytes requested until \n
        while (continue_reading_requested_bytes) {
            while (i < read_res) {
                if (buffer[i] == '\n') {
                    continue_reading_requested_bytes = 0;
                    i++;
                    break;
                }

                if (bytes_requested_index >= PATH_MAX - 1) {
                    invalid_command();
                }

                bytes_requested[bytes_requested_index] = buffer[i];
                bytes_requested_index++;
                i++;
            }
            if (continue_reading_requested_bytes == 0) {
                break;
            }

            // read next chunk
            read_res = read(STDIN_FILENO, buffer, sizeof(buffer));

            // check for errors from read()
            if (read_res < 0) {
                error();
            }
            if (read_res == 0) {
                invalid_command();
            }

            // reset i to keep reading
            i = 0;
        }

        if (bytes_requested_index == 0) {
            invalid_command();
        }

        bytes_requested[bytes_requested_index] = '\0';

        // check if the values read are numbers
        for (int j = 0; j < bytes_requested_index; j++) {
            if (!isdigit(bytes_requested[j])) {
                invalid_command();
            }
        }

        errno = 0;
        // cast string to int
        unsigned long long int content_length = strtoull(bytes_requested, NULL, 10);

        if (errno == ERANGE) {
            invalid_command();
        }

        // open the fd, in either write, create or truncate
        int set_fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0664);

        if (set_fd < 0) {
            error();
        }

        // keep writing to the set_fd
        unsigned long long int bytes_written = 0;
        while (bytes_written < content_length) {
            if (i >= read_res) {
                read_res = read(STDIN_FILENO, buffer, sizeof(buffer));

                if (read_res < 0) {
                    close(set_fd);
                    error();
                }

                if (read_res == 0) {
                    break;
                }
                i = 0;
            }

            size_t available_bytes = read_res - i;
            unsigned long long remaining_bytes = content_length - bytes_written;
            size_t bytes_to_write = available_bytes;

            if (bytes_to_write > remaining_bytes) {
                bytes_to_write = remaining_bytes;
            }

            ssize_t write_res = write(set_fd, buffer + i, bytes_to_write);

            if (write_res <= 0) {
                close(set_fd);
                error();
            }

            bytes_written += write_res;
            i += write_res;
        }

        int close_res = close(set_fd);
        if (close_res < 0) {
            error();
        }

        //success output
        write(STDOUT_FILENO, "OK\n", 3);
    } else {
        invalid_command();
    }
    return 0;
}
