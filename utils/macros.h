/**
 * macros.h
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef TERRAINER_MACROS_H
#define TERRAINER_MACROS_H

#ifdef TERRAINER_MODULE
#define SERVER_FREE(server, rid) server->free(rid)
#elif TERRAINER_GDEXTENSION
#define SERVER_FREE(server, rid) server->free_rid(rid)
#endif


#endif // TERRAINER_MACROS_H
