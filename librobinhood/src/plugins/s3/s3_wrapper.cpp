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
#include <chrono>
#include <ctime>

#include "s3_wrapper.h"

#define SIGNING_POLICY Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy
#define S3CLIENT Aws::S3::S3Client

struct metadata {
    Aws::Map<Aws::String, Aws::String> user_meta;
    size_t user_meta_size;
    time_t mtime;
    size_t size;
};

std::unique_ptr<S3CLIENT> s3_client_ptr = nullptr;
std::unique_ptr<metadata> s3_metadata_ptr = nullptr;
std::unique_ptr<map_entry> entry_ptr = nullptr;
std::shared_ptr<Aws::SDKOptions> options_ptr =
    std::make_shared<Aws::SDKOptions>();

/*----------------------------------------------------------------------------*
 |                                      API                                   |
 *----------------------------------------------------------------------------*/

void
s3_init_api(const char *address, const char *username, const char *password,
            const char *crt_path)
{
    Aws::InitAPI(*options_ptr);
    Aws::S3::S3ClientConfiguration config;
    config.endpointOverride = address;
    config.scheme = Aws::Http::Scheme::HTTPS;
    Aws::Auth::AWSCredentials credentials(username, password);
    config.caFile = crt_path;
    config.verifySSL = true;
    s3_client_ptr = std::unique_ptr<S3CLIENT>(new S3CLIENT(credentials,
                                              config, SIGNING_POLICY::Never,
                                              false));
    s3_metadata_ptr = std::unique_ptr<metadata>(new metadata());
}

void
s3_destroy_api()
{
    s3_client_ptr.reset();
    s3_metadata_ptr.reset();
    entry_ptr.reset();

    //The attribute destructor in rbh-sync cause this function to segfault
    //Temporarily commented out until a solution is found
    //Aws::ShutdownAPI(*options_ptr);
    options_ptr.reset();
}

/*----------------------------------------------------------------------------*
 |                                   Buckets                                  |
 *----------------------------------------------------------------------------*/

void
s3_get_bucket_list(size_t *number_of_buckets, char ***buckets_list)
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
             bucket = bucket_list[i];
             length = bucket.GetName().length();
             bucket_name_list[i] = new char[length + 1];
             std::strcpy(bucket_name_list[i], bucket.GetName().c_str());
        }
        *number_of_buckets = num_bucket;
        *buckets_list = bucket_name_list;
    } else {
        *number_of_buckets = -1;
        *buckets_list = NULL;
    }
}

/*----------------------------------------------------------------------------*
 |                                   Objects                                  |
 *----------------------------------------------------------------------------*/

void
s3_get_object_list(const char *bucket_name, size_t *object_list_length,
                   char ***list_objects)
{
    Aws::S3::Model::ListObjectsV2Request request;
    Aws::Vector<Aws::String> objects_vector;
    Aws::String continuationToken;
    char **results;

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

    if (*object_list_length == 0) {
        *list_objects = NULL;
        return;
    }

    results = new char*[*object_list_length];
    for (size_t i = 0; i < *object_list_length; i++) {
        results[i] = new char[objects_vector[i].length() + 1];
        std::strcpy(results[i], objects_vector[i].c_str());
    }
    *list_objects = results;
}

/*----------------------------------------------------------------------------*
 |                                   Metadata                                 |
 *----------------------------------------------------------------------------*/

bool
s3_create_metadata(const char *bucket_name,
                   const char *object_name)
{
    Aws::S3::Model::HeadObjectRequest request;
    request.SetBucket(bucket_name);
    request.SetKey(object_name);

    auto outcome = s3_client_ptr->HeadObject(request);
    if (outcome.IsSuccess()) {
        Aws::Map<Aws::String, Aws::String> custom_meta;
        std::chrono::seconds mtime_s;
        long long mtime_ms;
        size_t size;

        auto results = outcome.GetResultWithOwnership();
        size = results.GetContentLength();
        mtime_ms = results.GetLastModified().CurrentTimeMillis();
        mtime_s = std::chrono::seconds(mtime_ms / 1000);

        custom_meta = results.GetMetadata();

        s3_metadata_ptr->user_meta_size = custom_meta.size();
        s3_metadata_ptr->user_meta = custom_meta;
        s3_metadata_ptr->mtime = mtime_s.count();;
        s3_metadata_ptr->size = size;
    }

    return !outcome.IsSuccess();
}

time_t
s3_get_mtime()
{
    return s3_metadata_ptr->mtime;
}

size_t
s3_get_size()
{
    return s3_metadata_ptr->size;
}

size_t
s3_get_custom_size()
{
    return s3_metadata_ptr->user_meta_size;
}

struct map_entry*
s3_get_user_metadata_entry()
{
    auto mapIterator = s3_metadata_ptr->user_meta.begin();

    if (mapIterator == s3_metadata_ptr->user_meta.end()) {
        return NULL;
    } else {
        entry_ptr = std::unique_ptr<map_entry>(new struct map_entry);
        entry_ptr->key = mapIterator->first.c_str();
        entry_ptr->value = mapIterator->second.c_str();
    }

    ++mapIterator;
    return entry_ptr.get();
}

/*----------------------------------------------------------------------------*
 |                              Memory Management                             |
 *----------------------------------------------------------------------------*/

void
s3_delete_list(size_t list_length, char **list)
{
    if (list != NULL) {
        for (size_t i = 0; i < list_length; i++) {
            if (list[i] != NULL)
                delete[] list[i];
        }
        delete[] list;
    }
}
