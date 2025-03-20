/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/HeadObjectResult.h>
#include <aws/s3/model/ListObjectsV2Request.h>
#include <aws/s3/model/Object.h>
#include <aws/s3/S3Client.h>

#include <iostream>
#include <fstream>
#include <memory>
#include <string>

#include "s3_wrapper.h"

#define SIGNING_POLICY Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy
#define S3CLIENT Aws::S3::S3Client

std::unique_ptr<S3CLIENT> s3_client_ptr = nullptr;

/*----------------------------------------------------------------------------*
 |                                      API                                   |
 *----------------------------------------------------------------------------*/

void
s3_init_api(const char* address, const char* username, const char* password)
{
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    {
        Aws::S3::S3ClientConfiguration config;
        config.endpointOverride = address;
        config.scheme = Aws::Http::Scheme::HTTPS;
        Aws::Auth::AWSCredentials credentials(username, password);
        s3_client_ptr = std::unique_ptr<S3CLIENT>(new S3CLIENT(credentials,
                                                  config, SIGNING_POLICY::Never,
                                                  false));
    }
}

void
s3_destroy_api()
{
    s3_client_ptr.reset();
    Aws::SDKOptions options;
    Aws::ShutdownAPI(options);
}

/*----------------------------------------------------------------------------*
 |                                   Buckets                                  |
 *----------------------------------------------------------------------------*/

void
get_bucket_list(size_t *number_of_buckets, char ***buckets_list)
{
    Aws::Vector<Aws::S3::Model::Bucket> bucket_list;
    Aws::S3::Model::Bucket bucket;
    char **bucket_name_list;
    size_t num_bucket;
    size_t length;

    auto outcome = s3_client_ptr->ListBuckets();
    if (outcome.IsSuccess()) {

        bucket_list = outcome.GetResult().GetBuckets();
        num_bucket = bucket_list.size();
        bucket_name_list = new char*[num_bucket];

        for (size_t i = 0; i < num_bucket; ++i) {
             length = bucket.GetName().length();
             bucket = bucket_list[i];
             bucket_name_list[i] = new char[length + 1];
             std::strcpy(bucket_name_list[i], bucket.GetName().c_str());
        }
        *number_of_buckets = num_bucket;
        *buckets_list = bucket_name_list;
    }else{
        *number_of_buckets = 0;
        *buckets_list = NULL;

    }
}

void
get_object_list(const char* bucket_name, size_t *object_list_length,
                char ***list_objects)
{
    Aws::S3::Model::ListObjectsV2Request request;
    Aws::Vector<Aws::String> objects_vector;
    Aws::String continuationToken;
    char** results;

    request.WithBucket(bucket_name);

    do {
        if (!continuationToken.empty())
            request.SetContinuationToken(continuationToken);

        auto outcome = s3_client_ptr->ListObjectsV2(request);

        if (outcome.IsSuccess()) {
            Aws::Vector<Aws::S3::Model::Object> objects =
                    outcome.GetResult().GetContents();

            for (const auto& object : objects) {
                objects_vector.push_back(object.GetKey());
            }

            continuationToken = outcome.GetResult().GetNextContinuationToken();
        }

    } while (!continuationToken.empty());

    *object_list_length = objects_vector.size();
    results = new char*[*object_list_length];

    for (size_t i = 0; i < *object_list_length; i++) {
        results[i] = new char[objects_vector[i].length() + 1];
        std::strcpy(results[i], objects_vector[i].c_str());
    }
    *list_objects = results;
}
