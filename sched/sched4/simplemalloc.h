/*
 *   This file is part of Cooperative-LibEvent.
 *
 *   Cooperative-LibEvent is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Cooperative-LibEvent is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with Cooperative-LibEvent.  If not, see <http://www.gnu.org/licenses/>.
 */

/* 
 * File:   simplemalloc.h
 * Author: Michele Dionisio
 *
 * Created on 30 may 2015
 */

#ifndef __SIMPLEMALLOC_H__
#define __SIMPLEMALLOC_H__

#include <stddef.h>

size_t simple_sizeof(void *p);

void * simple_malloc();

void simple_free(void **);

void simple_compact(unsigned int);

#endif