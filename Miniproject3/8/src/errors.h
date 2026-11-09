#ifndef ERRORS_H
#define ERRORS_H

#include <esp_check.h>

#define ERROR 1
#define OK 0

enum operation_result {
  OPERATION_RESULT_ERROR = ERROR,
  OPERATION_RESULT_OK = OK
};

typedef enum operation_result operation_result_t;

/* The esp_err_t variants append ": <esp_err_to_name(x)>" to the message. */
#define OPERATION_RETURN_ON_ERROR(x, log_tag, format, ...)                \
  do {                                                                    \
    esp_err_t err_rc_ = (x);                                              \
    if (unlikely(err_rc_ != ESP_OK)) {                                    \
      ESP_LOGE(log_tag, "%s(%d): " format ": %s", __FUNCTION__, __LINE__, \
               ##__VA_ARGS__, esp_err_to_name(err_rc_));                  \
      return OPERATION_RESULT_ERROR;                                      \
    }                                                                     \
  } while (0)

#define OPERATION_PROPAGATE_OPERATION_ERROR(x)                 \
  do {                                                         \
    operation_result_t result_rc_ = (x);                       \
    if (unlikely(result_rc_ != OPERATION_RESULT_OK)) {         \
      return result_rc_;                                       \
    }                                                          \
  } while (0)

#define OPERATION_RETURN_ON_ERROR_WITH_CALLBACK(x, callback, log_tag, format, \
                                                ...)                          \
  do {                                                                        \
    esp_err_t err_rc_ = (x);                                                  \
    if (unlikely(err_rc_ != ESP_OK)) {                                        \
      ESP_LOGE(log_tag, "%s(%d): " format ": %s", __FUNCTION__, __LINE__,     \
               ##__VA_ARGS__, esp_err_to_name(err_rc_));                      \
      callback();                                                             \
      return OPERATION_RESULT_ERROR;                                          \
    }                                                                         \
  } while (0)

#define OPERATION_RETURN_ON_OPERATION_ERROR_WITH_CALLBACK( \
    x, callback, log_tag, format, ...)                     \
  do {                                                     \
    operation_result_t result_rc_ = (x);                   \
    if (unlikely(result_rc_ != OPERATION_RESULT_OK)) {     \
      ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__,   \
               __LINE__, ##__VA_ARGS__);                   \
      callback();                                          \
      return OPERATION_RESULT_ERROR;                       \
    }                                                      \
  } while (0)

#define OPERATION_RETURN_VOID_ON_ERROR(x, log_tag, format, ...) \
  ESP_RETURN_VOID_ON_ERROR(x, log_tag, format, ##__VA_ARGS__)

#endif  // ERRORS_H