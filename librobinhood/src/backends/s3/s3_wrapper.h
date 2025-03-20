/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef S3_WRAPPER_H_
#define S3_WRAPPER_H_

/**
 * Since the AWS SDK API for s3 has been written in cpp, this wrapper has
 * been made to make it compatible with the rbh backend in c
 */


#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Initialize the aws client
     *
     * s3_init_api is called to initialize the AWS API and generate a client
     * using the credentials given
     *
     * @param address   the s3 server address (ex: 127.0.0.1:9000)
     * @param username  the s3 username
     * @param password  the s3 password
     */
    void
    s3_init_api(const char *address, const char *username,
                const char *password);

    /**
     * Shutdown the AWS API
     */
    void
    s3_destroy_api();

    /**
    * Gives an array of the names of the buckets, along with
    * the length of the array
    *
    * @param number_buckets     the number of buckets on the server
    * @param bucket_list        the list of the names of the buckets
    */
    void
    get_bucket_list(size_t *number_of_buckets, char ***buckets_list);

    /**
    * Gives an array of the names of the objects contained in a bucket, along
    * with the length of the array
    *
    * @param bucket_name          the name of the bucket from which the objects
    *                             are being retrieved
    * @param object_list_length   the number of objects in the specified bucket
    * @param list_objects         the list of the names of the objects
    */
    void
    get_object_list(const char* bucket_name, size_t *object_list_length,
                    char ***list_objects);

#ifdef __cplusplus
}
#endif

#endif
