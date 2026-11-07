/* Filename and descriptive helper name are ours. The component path follows
 * the recovered CRYPTOCME/common/key source paths.
 */
#ifndef LIBSSL_CRYPTOCME_KEY_API_H
#define LIBSSL_CRYPTOCME_KEY_API_H

int sslDestroyKey(void *key);
int sslGetKeyInfo(void *key, int selector, void *result);
int sslSetKeyInfo(void *key, int selector, void *value);

#endif
