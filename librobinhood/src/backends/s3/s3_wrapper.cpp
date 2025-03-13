#include <iostream>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/core/auth/AWSCredentialsProvider.h>

#include <string>

#include "s3_wrapper.h"

Aws::S3::S3Client* s3_client_ptr = nullptr;

void
Init_API(const char* address, const char* username, const char* password)
{
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    {
        Aws::S3::S3ClientConfiguration config;
        config.endpointOverride = address;
        config.scheme = Aws::Http::Scheme::HTTP;
        Aws::Auth::AWSCredentials credentials(username, password);
        static Aws::S3::S3Client s3_client(credentials, config,
               Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
               false);
        s3_client_ptr = &s3_client;
    }
}

void
Destroy_API()
{
    Aws::SDKOptions options;
    Aws::ShutdownAPI(options);
}
