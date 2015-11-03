/* Copyright (c) 2015, Diego Souza
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
  
#include "erl_nif.h"
#include <magick/api.h>
#include <stdio.h>
#include <string.h>
#include <langinfo.h>

#define EXM_FAIL(j, m) do { errmsg = m; goto j; } while (0)

typedef struct {
  Image *image;
  ImageInfo *i_info;
  ExceptionInfo e_info;
} exm_resource_t;

static int exmagick_load (ErlNifEnv *env, void **data, ERL_NIF_TERM info);
static void exmagick_unload (ErlNifEnv *env, void *data);
static void exmagick_destroy (ErlNifEnv *env, void *data);
static char *exmagick_utf8strcpy(char *dst, ErlNifBinary *utf8, size_t len);
static int exmagick_get_utf8str (ErlNifEnv *env, ERL_NIF_TERM arg, ErlNifBinary *utf8);
static ERL_NIF_TERM exmagick_make_utf8str (ErlNifEnv *env, const char *data);
static ERL_NIF_TERM exmagick_image (ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM exmagick_image_load (ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM exmagick_image_dump (ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]);

ErlNifFunc exmagick_interface[] =
{
  {"image", 0, exmagick_image},
  {"image_load", 2, exmagick_image_load},
  {"image_dump", 2, exmagick_image_dump}
};

ERL_NIF_INIT(Elixir.ExMagick, exmagick_interface, exmagick_load, NULL, NULL, exmagick_unload)

static
int exmagick_load (ErlNifEnv *env, void **data, const ERL_NIF_TERM info)
{
  void *type = enif_open_resource_type(env, "Elixir", "ExMagick", exmagick_destroy, ERL_NIF_RT_CREATE, NULL);
  if (type == NULL)
  { return(-1); }
  InitializeMagick(NULL);
  *data = type;
  return(0);
}

static
void exmagick_destroy (ErlNifEnv *env, void *data)
{
  exm_resource_t *resource = (exm_resource_t *) data;
  if (resource->image != NULL)
  { DestroyImage(resource->image); }

  if (resource->i_info != NULL)
  { DestroyImageInfo(resource->i_info); }

  resource->image  = NULL;
  resource->i_info = NULL;
}

static
void exmagick_unload (ErlNifEnv *env, void *priv_data)
{}

static
char *exmagick_utf8strcpy (char *dst, ErlNifBinary *utf8, size_t size)
{
  size_t rsize = utf8->size >= size ? size - 1 : utf8->size;
  memcpy(dst, utf8->data, rsize);
  dst[rsize] = '\0';
  return dst;
}

static
ERL_NIF_TERM exmagick_make_utf8str (ErlNifEnv *env, const char *data)
{
  if (data == NULL)
  { return(enif_make_string(env, "<<NULL>>", ERL_NIF_LATIN1)); }
  else
  { return(enif_make_string(env, data, ERL_NIF_LATIN1)); }
}

static
int exmagick_get_utf8str (ErlNifEnv *env, const ERL_NIF_TERM arg, ErlNifBinary *utf8)
{ return(enif_inspect_binary(env, arg, utf8)); }

static
ERL_NIF_TERM exmagick_image (ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  ERL_NIF_TERM result;

  char *errmsg             = NULL;
  ErlNifResourceType *type = (ErlNifResourceType *) enif_priv_data(env);
  exm_resource_t *resource = enif_alloc_resource(type, sizeof(exm_resource_t));
  if (resource == NULL)
  { EXM_FAIL(ehandler, "enif_alloc_resource"); }

  resource->image  = NULL;
  resource->i_info = CloneImageInfo(0);
  GetExceptionInfo(&resource->e_info);
  if (resource->i_info == NULL)
  { EXM_FAIL(ehandler, "CloneImageInfo"); }

  result = enif_make_resource(env, (void *) resource);
  enif_release_resource(resource);
  return(enif_make_tuple2(env, enif_make_atom(env, "ok"), result));

ehandler:
  if (resource != NULL)
  {
    if (resource->i_info != NULL)
    { DestroyImageInfo(resource->i_info); }
    enif_release_resource(resource);
  }
  return(enif_make_tuple2(env, enif_make_atom(env, "error"), exmagick_make_utf8str(env, errmsg)));
}

static
ERL_NIF_TERM exmagick_image_load (ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  ErlNifBinary utf8;
  exm_resource_t *resource;

  char *errmsg             = NULL;
  ErlNifResourceType *type = (ErlNifResourceType *) enif_priv_data(env);

  if (0 == enif_get_resource(env, argv[0], type, (void **) &resource))
  { EXM_FAIL(ehandler, "argv[0]: bad argument"); }

  if (0 == exmagick_get_utf8str(env, argv[1], &utf8))
  { EXM_FAIL(ehandler, "argv[1]: bad argument"); }

  exmagick_utf8strcpy(resource->i_info->filename, &utf8, MaxTextExtent);
  if (resource->image != NULL)
  {
    DestroyImage(resource->image);
    resource->image = NULL;
  }
  
  resource->image = ReadImage(resource->i_info, &resource->e_info);
  if (resource->image == NULL)
  {
    CatchException(&resource->e_info);
    EXM_FAIL(ehandler, resource->e_info.reason);
  }
  
  return(enif_make_atom(env, "ok"));

ehandler:
  return(enif_make_tuple2(env, enif_make_atom(env, "error"), exmagick_make_utf8str(env, errmsg)));
}

static
ERL_NIF_TERM exmagick_image_dump (ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  ErlNifBinary utf8;
  exm_resource_t *resource;

  char *errmsg             = NULL;
  ErlNifResourceType *type = (ErlNifResourceType *) enif_priv_data(env);

  if (0 == enif_get_resource(env, argv[0], type, (void **) &resource))
  { EXM_FAIL(ehandler, "argv[0]: bad argument"); }

  if (0 == exmagick_get_utf8str(env, argv[1], &utf8))
  { EXM_FAIL(ehandler, "argv[1]: bad argument"); }

  exmagick_utf8strcpy(resource->image->filename, &utf8, MaxTextExtent);
  if (0 == WriteImage(resource->i_info, resource->image))
  {
    CatchException(&resource->image->exception);
    EXM_FAIL(ehandler, resource->image->exception.reason);
  }

  return(enif_make_atom(env, "ok"));

ehandler:
  return(enif_make_tuple2(env, enif_make_atom(env, "error"), exmagick_make_utf8str(env, errmsg)));
}
