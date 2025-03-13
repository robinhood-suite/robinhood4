/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <iostream>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/core/auth/AWSCredentialsProvider.h>

#include <memory>
#include <string>

#include "s3_wrapper.h"

#define SIGNING_POLICY Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy
#define S3CLIENT Aws::S3::S3Client

std::unique_ptr<Aws::S3::S3Client> s3_client_ptr = nullptr;

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
        s3_client_ptr = std::unique_ptr<S3CLIENT>(new S3CLIENT(credentials,
                                                  config, SIGNING_POLICY::Never,
                                                  false));
    }
}

void
Destroy_API()
{
    s3_client_ptr.reset();
    Aws::SDKOptions options;
    Aws::ShutdownAPI(options);
}
