/**
 * @file nvm_driver.h
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief
 * @version 0.1
 * @date 2025-05-28
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef NVM_DRIVER_H
#define NVM_DRIVER_H

#include <stdint.h>

void nvm_init();
void nvm_write(const char *key, uint32_t value);
void nvm_read(const char *key, uint32_t *value);
void nvm_erase(const char *key);
void nvm_erase_all();

#endif  // NVM_DRIVER_H
