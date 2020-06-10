#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include "ockam/error.h"
#include "ockam/syslog.h"
#include "ockam/io.h"
#include "ockam/transport.h"
#include "tests.h"
#include "ockam/memory.h"
#include "memory/stdlib/stdlib.h"

#define DEFAULT_FIXTURE_PATH  "fixtures"
#define DEFAULT_IP_ADDRESS    "127.0.0.1"
#define DEFAULT_IP_PORT       8000
#define FIXTURE_PATH_LEN      192
#define FIXTURE_FULL_PATH_LEN 256

char* p_file_to_send    = "client_test_data.txt";
char* p_file_to_receive = "server_data_received.txt";
char* p_file_to_compare = "server_test_data.txt";

ockam_transport_tcp_socket_attributes_t transport_attributes;

int test_tcp_client(ockam_ip_address_t* address, char* p_fixture_path)
{
  int               error = TRANSPORT_ERROR_TEST;
  ockam_transport_t transport;
  ockam_reader_t*   p_transport_reader;
  ockam_writer_t*   p_transport_writer;
  uint8_t           send_buffer[64];
  size_t            send_length;
  uint8_t           receive_buffer[64];
  size_t            bytes_received  = 0;
  FILE*             file_to_send    = NULL;
  FILE*             file_to_receive = NULL;
  size_t            bytes_written;
  uint16_t          send_not_done                               = 1;
  uint16_t          receive_not_done                            = 1;
  char              file_to_send_path[FIXTURE_FULL_PATH_LEN]    = { 0 };
  char              file_to_receive_path[FIXTURE_FULL_PATH_LEN] = { 0 };
  char              file_to_compare_path[FIXTURE_FULL_PATH_LEN] = { 0 };
  ockam_memory_t    ockam_memory                                = { 0 };

  // Open the test data file for sending
  sprintf(&file_to_send_path[0], "%s/%s", p_fixture_path, p_file_to_send);
  file_to_send = fopen(&file_to_send_path[0], "r");
  if (NULL == file_to_send) {
    log_error(error, "failed to open client test data");
    goto exit;
  }
  // Create file for test data received
  sprintf(&file_to_receive_path[0], "%s/%s", p_fixture_path, p_file_to_receive);
  file_to_receive = fopen(&file_to_receive_path[0], "w");
  if (NULL == file_to_send) {
    log_error(error, "failed to open client_data_received.txt");
    goto exit;
  }

  error = ockam_memory_stdlib_init(&ockam_memory);
  if (error) goto exit;

  ockam_memory_set(&ockam_memory, &transport_attributes, 0, sizeof(transport_attributes));
  transport_attributes.p_memory = &ockam_memory;
  error                         = ockam_transport_socket_tcp_init(&transport, &transport_attributes);
  if (error) goto exit;
  error = ockam_transport_connect(&transport, &p_transport_reader, &p_transport_writer, address, 10, 1);
  if (error) goto exit;

  int i = 0;
  while (send_not_done) {
    send_length = fread(&send_buffer[0], 1, sizeof(send_buffer), file_to_send);
    if (feof(file_to_send)) send_not_done = 0;
    ++i;
    error = ockam_write(p_transport_writer, send_buffer, send_length);
    if (error) {
      log_error(error, "Send failed");
      goto exit;
    }
  }

  // Send special "the end" buffer
  error = ockam_write(p_transport_writer, (uint8_t*) "that's all", strlen("that's all") + 1);
  if (error) {
    log_error(error, "Send failed");
    goto exit;
  }

  // Receive the test data file
  while (receive_not_done) {
    error = ockam_read(p_transport_reader, &receive_buffer[0], sizeof(receive_buffer), &bytes_received);
    if (TRANSPORT_ERROR_NONE != error) {
      log_error(error, "Receive failed");
      goto exit;
    }
    // Look for special "the end" buffer
    if (0 == strncmp("that's all", (char*) receive_buffer, strlen("that's all"))) {
      receive_not_done = 0;
    } else {
      bytes_written = fwrite(&receive_buffer[0], 1, bytes_received, file_to_receive);
      if (bytes_written != bytes_received) {
        log_error(TRANSPORT_ERROR_TEST, "failed write to output file");
        goto exit;
      }
    }
  }

  fclose(file_to_send);
  fclose(file_to_receive);

  // Now compare the received file and the reference file
  sprintf(file_to_compare_path, "%s/%s", p_fixture_path, p_file_to_compare);
  if (0 != file_compare(file_to_receive_path, file_to_compare_path)) {
    error = TRANSPORT_ERROR_TEST;
    log_error(error, "file compare failed");
    goto exit;
  }
  printf("Client test successful!\n");

exit:
  return error;
}
