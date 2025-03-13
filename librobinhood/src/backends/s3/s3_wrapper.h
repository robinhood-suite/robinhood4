#ifndef S3_WRAPPER_H_
#define S3_WRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

    void
    Init_API(const char* address, const char* username,
             const char* password);

    void
    Destroy_API();

#ifdef __cplusplus
}
#endif

#endif
