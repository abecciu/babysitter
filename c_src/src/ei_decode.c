
#include "ei_decode.h"
#include "process_manager.h"

/**
* Decode the arguments into a new_process
*
* @params
* {Cmd::string(), [Option]}
*     Option = {env, Strings} | {cd, Dir} | {do_before, Cmd} | {do_after, Cmd} | {nice, int()}
**/
int decode_command_call_into_process(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[], process_t **ptr)
{
  // Instantiate a new process
  if (pm_new_process(ptr))
    return -1;
  
  process_t *process = *ptr;
  const ERL_NIF_TERM* big_tuple;
  int arity = 2;
  // Get the outer tuple
  if(!enif_get_tuple(env, argv[0], &arity, &big_tuple)) return -1;
  
  // The first command is a string
  char command[MAX_BUFFER_SZ], key[MAX_BUFFER_SZ], value[MAX_BUFFER_SZ];
  memset(&command, '\0', sizeof(command));
  
  // Get the command
  if (enif_get_string(env, big_tuple[0], command, sizeof(command), ERL_NIF_LATIN1) < 0) return -1;
  pm_malloc_and_set_attribute(&process->command, command);
  
  // The second element of the tuple is a list of options
  const ERL_NIF_TERM* tuple;
  ERL_NIF_TERM head, tail, list = big_tuple[1];
  
  // int enif_get_tuple(ErlNifEnv* env, ERL_NIF_TERM term, int* arity, const ERL_NIF_TERM** array)
  while(enif_get_list_cell(env, list, &head, &tail)) {
    // Get the tuple
    if(!enif_get_tuple(env, head, &arity, &tuple)) return -1;
    // First element is an atom
    if (!enif_get_atom(env, tuple[0], key, sizeof(key))) return -2;
    if (enif_get_string(env, tuple[1], value, sizeof(value), ERL_NIF_LATIN1) < 0) return -3;
    if (!strcmp(key, "do_before")) {
      // Do before
      pm_malloc_and_set_attribute(&process->before, value);
    } else if (!strcmp(key, "do_after")) {
      pm_malloc_and_set_attribute(&process->after, value);
    } else if (!strcmp(key, "cd")) {
      pm_malloc_and_set_attribute(&process->cd, value);
    } else if (!strcmp(key, "env")) {
      pm_add_env(&process, value);
    } else if (!strcmp(key, "nice")) {
      process->nice = atoi(value);
    }
    list = tail;
  }
  return 0;
}

/**
* Translate ei buffer into a process_t object
**/
int ei_decode_command_call_into_process(char *buf, process_t **ptr)
{
  // Instantiate a new process
  if (pm_new_process(ptr))
    return -1;
  
  int   arity, index, version;
  long  transId;
  char* buf;
  if ((buf = (char *) malloc( sizeof(buf) )) == NULL) return -1;
    
  // Reset the index, so that ei functions can decode terms from the 
  // beginning of the buffer
  index = 0;

  /* Ensure that we are receiving the binary term by reading and 
   * stripping the version byte */
  if (ei_decode_version(buf, &index, &version)) return -2;
  // Decode the tuple header and make sure that the arity is 2
  // as the tuple spec requires it to contain a tuple: {TransId, {Cmd::atom(), Arg1, Arg2, ...}}
  if (ei_decode_tuple_header(buf, &index, &arity) != 2) return -3; // decode the tuple and capture the arity
  if (ei_decode_long(buf, &index, &transId) < 0) return -4; // Get the transId
  if ((arity = ei_decode_tuple_header(buf, &index, &arity)) < 2) return -5; 
  
  process_t *process = *ptr;
  
  // Get the outer tuple  
  // The first command is a string
  char command[MAX_BUFFER_SZ], key[MAX_BUFFER_SZ], value[MAX_BUFFER_SZ];
  memset(&command, '\0', sizeof(command));
  
  // Get the command
  if (ei_decode_atom(buf, &index, command)) return -6;
  pm_malloc_and_set_attribute(&process->command, command);
  
  // The second element of the tuple is a list of options
  const ERL_NIF_TERM* tuple;
  ERL_NIF_TERM head, tail, list = big_tuple[1];
  
  // int enif_get_tuple(ErlNifEnv* env, ERL_NIF_TERM term, int* arity, const ERL_NIF_TERM** array)
  while(enif_get_list_cell(env, list, &head, &tail)) {
    // Get the tuple
    if(!enif_get_tuple(env, head, &arity, &tuple)) return -1;
    // First element is an atom
    if (!enif_get_atom(env, tuple[0], key, sizeof(key))) return -2;
    if (enif_get_string(env, tuple[1], value, sizeof(value), ERL_NIF_LATIN1) < 0) return -3;
    if (!strcmp(key, "do_before")) {
      // Do before
      pm_malloc_and_set_attribute(&process->before, value);
    } else if (!strcmp(key, "do_after")) {
      pm_malloc_and_set_attribute(&process->after, value);
    } else if (!strcmp(key, "cd")) {
      pm_malloc_and_set_attribute(&process->cd, value);
    } else if (!strcmp(key, "env")) {
      pm_add_env(&process, value);
    } else if (!strcmp(key, "nice")) {
      process->nice = atoi(value);
    }
    list = tail;
  }
  return 0;
}


//---- Decoders ----//
char read_buf[BUFFER_SZ];
int  read_index = 0;

void ei_list_to_string(ErlNifEnv *env, ERL_NIF_TERM list, char *string)
{
  ERL_NIF_TERM head, tail;
  int character;

  while(enif_get_list_cell(env, list, &head, &tail)) {
    if(!enif_get_int(env, head, &character)) {
      return;
    }
    
    *string++ = (char)character;
    list = tail;
  };

  *string = '\0';
};

char *ei_arg_list_to_string(ErlNifEnv *env, ERL_NIF_TERM list, int *arg_size)
{
  ERL_NIF_TERM head, tail;
  char str_length[PREFIX_LEN], *args;
  int i, length, character;

  for(i=0; i<PREFIX_LEN; i++) {
    if(enif_get_list_cell(env, list, &head, &tail)) {
      if(!enif_get_int(env, head, &character)) {
        return NULL;
      }
      str_length[i] = (char)character;
      list = tail;
    } else {
      return NULL;
    }
  };

  length = atoi(str_length)+1;
  args = (char *)calloc(length, sizeof(char));

  ei_list_to_string(env, list, args);
  *arg_size = length;

  return args;
};

/**
* ok
**/
ERL_NIF_TERM ok(ErlNifEnv* env, const char* atom, const char *fmt, ...)
{
  char str[MAXATOMLEN];
  va_list vargs;
  va_start (vargs, fmt);
  vsnprintf(str, sizeof(str), fmt, vargs);
  va_end   (vargs);
  
  return enif_make_tuple2(env, enif_make_atom(env,atom), enif_make_atom(env, str));
}

/**
* error
**/
ERL_NIF_TERM error(ErlNifEnv* env, const char *fmt, ...)
{
  char str[MAXATOMLEN];
  va_list vargs;
  va_start (vargs, fmt);
  vsnprintf(str, sizeof(str), fmt, vargs);
  va_end   (vargs);
  
  return enif_make_tuple2(env, enif_make_atom(env,"error"), enif_make_atom(env, str));
}


/**
* Data marshalling functions
**/
int ei_write_atom(int fd, const char* first, const char* fmt, ...)
{
  ei_x_buff result;
  if (ei_x_new_with_version(&result) || ei_x_encode_tuple_header(&result, 2)) return -1;
  if (ei_x_encode_atom(&result, first) ) return -2;
  // Encode string
  char str[MAXATOMLEN];
  va_list vargs;
  va_start (vargs, fmt);
  vsnprintf(str, sizeof(str), fmt, vargs);
  va_end   (vargs);
  
  if (ei_x_encode_string_len(&result, str, strlen(str))) return -3;
  
  write_cmd(&result, fd);
  ei_x_free(&result);
  return 0;
}

int ei_ok(int fd, const char* fmt, va_list vargs){return ei_write_atom(fd, "ok", fmt, vargs);}
int ei_error(int fd, const char* fmt, va_list vargs){return ei_write_atom(fd, "error", fmt, vargs);}

/**
* Data i/o
**/
int read_cmd(byte **buf, int *size, int fd)
{
  int len;
  if (read_exact(*buf, 2, fd) != 2) return(-1);
  len = (*buf[0] << 8) | *buf[1];

  if (len > *size) {
    byte* tmp = (byte *) realloc(*buf, len);
    if (tmp == NULL)
      return -1;
    else
      *buf = tmp;
      
    *size = len;
  }
  return read_exact(*buf, len, fd);
}

int write_cmd(ei_x_buff *buff, int fd)
{
  byte li;

  li = (buff->index >> 8) & 0xff; 
  write_exact(&li, 1, fd);
  li = buff->index & 0xff;
  write_exact(&li, 1, fd);

  return write_exact(buff->buff, buff->index, fd);
}

int read_exact(byte *buf, int len, int fd)
{
  int i, got=0;

  do {
    if ((i = read(fd, buf+got, len-got)) <= 0)
      return i;
    got += i;
  } while (got<len);

  return len;
}

int write_exact(byte *buf, int len, int fd)
{
  int i, wrote = 0;

  do {
    if ((i = write(fd, buf+wrote, len-wrote)) <= 0)
      return i;
    wrote += i;
  } while (wrote<len);

  return len;
}