/* 
 * ssheven
 * 
 * Copyright (c) 2021 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#pragma once

void ssh_write(char* buf, size_t len);
void* read_thread(void* arg);
