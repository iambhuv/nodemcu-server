#ifndef SIMPLE_HTTP_H
#define SIMPLE_HTTP_H

#include <stdbool.h>
#include <string.h>

typedef struct {
  char method[16];
  char path[128];
  bool valid;
} http_request_t;

// Parse HTTP request line: "GET /path HTTP/1.1\r\n"
static inline http_request_t http_parse_request(const char* buf, size_t len) {
  http_request_t req = {0};
  req.valid = false;

  if (len < 14)
    return req; // Minimum: "GET / HTTP/1.1"

  // Find first space (end of method)
  const char* space1 = memchr(buf, ' ', len);
  if (!space1)
    return req;

  size_t method_len = space1 - buf;
  if (method_len >= sizeof(req.method))
    return req;

  // Copy method
  memcpy(req.method, buf, method_len);
  req.method[method_len] = '\0';

  // Find second space (end of path)
  const char* space2 = memchr(space1 + 1, ' ', len - (space1 - buf + 1));
  if (!space2)
    return req;

  size_t path_len = space2 - (space1 + 1);
  if (path_len >= sizeof(req.path))
    return req;

  // Copy path
  memcpy(req.path, space1 + 1, path_len);
  req.path[path_len] = '\0';

  req.valid = true;
  return req;
}

// Parse path like "/relay/5" and extract the number
static inline int http_path_get_int(const char* path, const char* prefix) {
  size_t prefix_len = strlen(prefix);

  // Check if path starts with prefix
  if (strncmp(path, prefix, prefix_len) != 0) {
    return -1;
  }

  // Get the part after prefix
  const char* num_str = path + prefix_len;

  // Skip leading slash if present
  if (num_str[0] == '/') {
    num_str++;
  }

  // Empty or no number
  if (num_str[0] == '\0')
    return -1;

  // Parse the integer
  int result = 0;
  for (int i = 0; num_str[i] != '\0'; i++) {
    if (num_str[i] < '0' || num_str[i] > '9') {
      // Hit non-digit (could be '?' for query params)
      if (i == 0)
        return -1; // No digits at all
      break;       // Stop at first non-digit
    }
    result = result * 10 + (num_str[i] - '0');
  }

  return result;
}

#endif // SIMPLE_HTTP_H