/*
 * Optimized Ruby Mutex implementation, loosely based on thread.rb by
 * Yukihiro Matsumoto <matz@ruby-lang.org>
 *
 *  Copyright 2006  MenTaLguY <mental@rydia.net>
 *
 * This file is made available under the same terms as Ruby.
 */

#include <ruby.h>
#include <intern.h>
#include <rubysig.h>

static VALUE rb_cMutex;
static VALUE rb_cConditionVariable;
static VALUE rb_eThreadError;

static VALUE
return_value(value)
  VALUE value;
{
  return value;
}

typedef struct _Entry {
  VALUE value;
  struct _Entry *next;
} Entry;

typedef struct _List {
  Entry *entries;
  Entry *last_entry;
  Entry *entry_pool;
} List;

static void
init_list(list)
  List *list;
{
  list->entries = NULL;
  list->last_entry = NULL;
  list->entry_pool = NULL;
}

static void
mark_list(list)
  List *list;
{
  Entry *entry;
  for ( entry = list->entries ; entry ; entry = entry->next ) {
    rb_gc_mark(entry->value);
  }
}

static void
free_entries(first)
  Entry *first;
{
  Entry *next;
  while (first) {
    next = first->next;
    free(first);
    first = next;
  }
}

static void
finalize_list(list)
  List *list;
{
  free_entries(list->entries);
  free_entries(list->entry_pool);
  list->entries = NULL;
  list->last_entry = NULL;
  list->entry_pool = NULL;
}

static void
put_list(list, value)
  List *list;
  VALUE value;
{
  Entry *entry;

  if (list->entry_pool) {
    entry = list->entry_pool;
    list->entry_pool = entry->next;
  } else {
    entry = (Entry *)malloc(sizeof(Entry));
  }

  entry->value = value;
  entry->next = NULL;

  if (list->last_entry) {
    list->last_entry->next = entry;
  } else {
    list->entries = entry;
  }
  list->last_entry = entry;
}

static VALUE
get_list(list)
  List *list;
{
  Entry *entry;

  entry = list->entries;
  if (!entry) return Qundef;

  list->entries = entry->next;
  if ( entry == list->last_entry ) {
    list->last_entry = NULL;
  }
  entry->next = list->entry_pool;
  list->entry_pool = entry;

  return entry->value;
}

static VALUE
wake_one(list)
  List *list;
{
  VALUE waking;

  waking = Qnil;
  while ( list->entries && !RTEST(waking) ) {
    waking = rb_rescue2(rb_thread_wakeup, get_list(list),
                        return_value, Qnil, rb_eThreadError, 0);
  }

  return waking;
}

static VALUE
wake_all(list)
  List *list;
{
  while ( list->entries ) {
    wake_one(list);
  }
  return Qnil;
}

typedef struct _Mutex {
  VALUE owner;
  List waiting;
} Mutex;

static void
rb_mutex_mark(m)
  Mutex *m;
{
  rb_gc_mark(m->owner);
  mark_list(&m->waiting);
}

static void
rb_mutex_free(m)
  Mutex *m;
{
  if (m->waiting.entries) {
    rb_bug("mutex %p freed with thread(s) waiting", m);
  }
  finalize_list(&m->waiting);
  free(m);
}

static VALUE
rb_mutex_alloc()
{
  Mutex *m;
  m = (Mutex *)malloc(sizeof(Mutex));

  m->owner = Qfalse;
  init_list(&m->waiting);

  return Data_Wrap_Struct(rb_cMutex, rb_mutex_mark, rb_mutex_free, m);
}

static VALUE
rb_mutex_locked_p(self)
  VALUE self;
{
  Mutex *m;
  Data_Get_Struct(self, Mutex, m);
  return ( RTEST(m->owner) ? Qtrue : Qfalse );
}

static VALUE
rb_mutex_try_lock(self)
  VALUE self;
{
  Mutex *m;
  VALUE result;

  Data_Get_Struct(self, Mutex, m);

  result = Qfalse;

  rb_thread_critical = Qtrue;
  if (!RTEST(m->owner)) {
    m->owner = rb_thread_current();
    result = Qtrue;
  }
  rb_thread_critical = Qfalse;

  return result;
}

static VALUE
rb_mutex_lock(self)
  VALUE self;
{
  Mutex *m;
  VALUE current;

  Data_Get_Struct(self, Mutex, m);
  current = rb_thread_current();

  rb_thread_critical = Qtrue;
  while (RTEST(m->owner)) {
    if ( m->owner == current ) {
      rb_raise(rb_eThreadError, "deadlock; recursive locking");
    }

    put_list(&m->waiting, current);
    rb_thread_stop();

    rb_thread_critical = Qtrue;
  }
  m->owner = current; 
  rb_thread_critical = Qfalse;
  return self;
}

static VALUE
rb_mutex_unlock(self)
  VALUE self;
{
  Mutex *m;
  VALUE waking;
  Data_Get_Struct(self, Mutex, m);

  if (!RTEST(m->owner)) {
    return Qnil;
  }

  rb_thread_critical = Qtrue;
  m->owner = Qnil;
  waking = wake_one(&m->waiting);
  rb_thread_critical = Qfalse;

  if (RTEST(waking)) {
    rb_rescue2(rb_thread_run, waking, return_value, Qnil, rb_eThreadError, 0);
  }

  return self;
}

static VALUE
set_critical(value)
  VALUE value;
{
  rb_thread_critical = value;
  return Qnil;
}

static VALUE
rb_mutex_exclusive_unlock(self)
  VALUE self;
{
  Mutex *m;
  Data_Get_Struct(self, Mutex, m);

  if (!RTEST(m->owner)) {
    return Qnil;
  }

  rb_thread_critical = Qtrue;
  m->owner = Qnil;
  wake_one(&m->waiting);
  rb_ensure(rb_yield, Qundef, set_critical, Qfalse);

  return self;
}

static VALUE
rb_mutex_synchronize(self)
  VALUE self;
{
  rb_mutex_lock(self);
  return rb_ensure(rb_yield, Qundef, rb_mutex_unlock, self);
}

typedef struct _ConditionVariable {
  List waiting;
} ConditionVariable;

static void
rb_condvar_mark(condvar)
  ConditionVariable *condvar;
{
  mark_list(&condvar->waiting);
}

static void
rb_condvar_free(condvar)
  ConditionVariable *condvar;
{
  finalize_list(&condvar->waiting);
  free(condvar);
}

static VALUE
rb_condvar_alloc()
{
  ConditionVariable *condvar;

  condvar = (ConditionVariable *)malloc(sizeof(ConditionVariable));
  init_list(&condvar->waiting);

  return Data_Wrap_Struct(rb_cConditionVariable, rb_condvar_mark, rb_condvar_free, condvar);
}

static VALUE
rb_condvar_wait(VALUE self, VALUE mutex_v)
{
  ConditionVariable *condvar;
  Mutex *mutex;

  if ( CLASS_OF(mutex_v) != rb_cMutex ) {
    rb_raise(rb_eTypeError, "Not a Mutex");
  }
  Data_Get_Struct(self, ConditionVariable, condvar);
  Data_Get_Struct(mutex_v, Mutex, mutex);

  rb_thread_critical = Qtrue;

  if (!RTEST(mutex->owner)) {
    rb_thread_critical = Qfalse;
    return self;
  }

  mutex->owner = Qnil;
  put_list(&mutex->waiting, rb_thread_current());
  rb_thread_stop();

  rb_mutex_lock(mutex_v);

  return self;
}

static VALUE
rb_condvar_broadcast(self)
  VALUE self;
{
  ConditionVariable *condvar;

  Data_Get_Struct(self, ConditionVariable, condvar);
  
  rb_thread_critical = Qtrue;
  rb_ensure(wake_all, (VALUE)&condvar->waiting, set_critical, Qfalse);
  rb_thread_schedule();

  return self;
}

static VALUE
rb_condvar_signal(self)
  VALUE self;
{
  ConditionVariable *condvar;

  Data_Get_Struct(self, ConditionVariable, condvar);

  rb_thread_critical = Qtrue;
  rb_ensure(wake_one, (VALUE)&condvar->waiting, set_critical, Qfalse);
  rb_thread_schedule();

  return self;
}

void
Init_fastthread()
{
  rb_require("thread");

  rb_eThreadError = rb_const_get(rb_cObject, rb_intern("ThreadError"));

  rb_cMutex = rb_define_class("Mutex", rb_cObject);
  rb_define_alloc_func(rb_cMutex, rb_mutex_alloc);
  rb_define_method(rb_cMutex, "locked?", rb_mutex_locked_p, 0);
  rb_define_method(rb_cMutex, "try_lock", rb_mutex_try_lock, 0);
  rb_define_method(rb_cMutex, "lock", rb_mutex_lock, 0);
  rb_define_method(rb_cMutex, "unlock", rb_mutex_unlock, 0);
  rb_define_method(rb_cMutex, "exclusive_unlock", rb_mutex_exclusive_unlock, 0);
  rb_define_method(rb_cMutex, "synchronize", rb_mutex_synchronize, 0);

  rb_cConditionVariable = rb_define_class("ConditionVariable", rb_cObject);
  rb_define_alloc_func(rb_cConditionVariable, rb_condvar_alloc);
  rb_define_method(rb_cConditionVariable, "wait", rb_condvar_wait, 1);
  rb_define_method(rb_cConditionVariable, "broadcast", rb_condvar_broadcast, 0);
  rb_define_method(rb_cConditionVariable, "signal", rb_condvar_signal, 0);
}

