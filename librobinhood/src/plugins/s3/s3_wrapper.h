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

struct map_entry {
    const char *key;
    const char *value;
};

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
     * @param crt_path  the path to the crt file
     */
    void
    s3_init_api(const char *address, const char *username,
                const char *password, const char *crt_path);

    /**
     * Shutdown the AWS API
     */
    void
    s3_destroy_api();

    /**
     * Gives an array of the names of the buckets, along with
     * the length of the array
     *
     * @param number_of_buckets     the number of buckets on the server
     * @param buckets_list          the list of the names of the buckets
     */
    void
    s3_get_bucket_list(size_t *number_of_buckets, char ***buckets_list);

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
    s3_get_object_list(const char *bucket_name, size_t *object_list_length,
                       char ***list_objects);

    /**
     * Free an array declared in c++ (used for bucket and object lists)
     *
     * @param list_length    the length of the array
     * @param list           the list to delete
     */
    void
    s3_delete_list(size_t list_length, char **list);

    /**
     * Create the metadata struct for a given object and bucket.
     * Needs to be called before any of the following s3_get functions
     *
     * @param bucket_name   the name of the bucket
     * @param object_name   the name of the object
     *
     * @return              0 if successfully retrieved the metadata,
     *                      1 otherwise
     */
    bool
    s3_create_metadata(const char *bucket_name,
                       const char *object_name);

    /**
     * Return the modified time from the metadata struct created previously
     *
     * @return  the mtime in seconds
     */
    time_t
    s3_get_mtime();

    /**
     * Return the size of the file from the metadata struct created previously
     *
     * @return  the size of the file
     */
    size_t
    s3_get_size();

    /**
     * Return a single entry of the map containing the user metadata from
     * the metadata struct created previously
     *
     * @return  a map_entry struct containing the key and value, return NULL
     *          when there is no more entry to retrieve
     */
    struct map_entry*
    s3_get_user_metadata_entry();

    /**
     * Return the size of the user metadata map from the metadata
     * struct created previously
     *
     * @return  the number of entries in the user metadata map
     */
    size_t
    s3_get_custom_size();

#ifdef __cplusplus
}
#endif

#endif
