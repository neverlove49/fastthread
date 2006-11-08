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
static VALUE rb_eThreadError;

typedef struct _Entry {
  VALUE value;
  struct _Entry *next;
} Entry;

typedef struct _Queue {
  Entry *entries;
  Entry *last_entry;
  Entry *entry_pool;
} Queue;

typedef struct _Mutex {
  VALUE owner;
  Queue waiting;
} Mutex;

static void
init_queue(queue)
  Queue *queue;
{
  queue->entries = NULL;
  queue->last_entry = NULL;
  queue->entry_pool = NULL;
}

static void
mark_queue(queue)
  Queue *queue;
{
  Entry *entry;
  for ( entry = queue->entries ; entry ; entry = entry->next ) {
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
finalize_queue(queue)
  Queue *queue;
{
  free_entries(queue->entries);
  free_entries(queue->entry_pool);
  queue->entries = NULL;
  queue->last_entry = NULL;
  queue->entry_pool = NULL;
}

static void
put_queue(queue, value)
  Queue *queue;
  VALUE value;
{
  Entry *entry;

  if (queue->entry_pool) {
    entry = queue->entry_pool;
    queue->entry_pool = entry->next;
  } else {
    entry = (Entry *)malloc(sizeof(Entry));
  }

  entry->value = value;
  entry->next = NULL;

  if (queue->last_entry) {
    queue->last_entry->next = entry;
  } else {
    queue->entries = entry;
  }
  queue->last_entry = entry;
}

static VALUE
get_queue(queue)
  Queue *queue;
{
  Entry *entry;

  entry = queue->entries;
  if (!entry) return Qundef;

  queue->entries = entry->next;
  if ( entry == queue->last_entry ) {
    queue->last_entry = NULL;
  }
  entry->next = queue->entry_pool;
  queue->entry_pool = entry;

  return entry->value;
}

static void
rb_mutex_mark(m)
  Mutex *m;
{
  rb_gc_mark(m->owner);
  mark_queue(&m->waiting);
}

static void
rb_mutex_free(m)
  Mutex *m;
{
  if (m->waiting.entries) {
    rb_bug("mutex %p freed with thread(s) waiting", m);
  }
  finalize_queue(&m->waiting);
  free(m);
}

static VALUE
return_value(value)
  VALUE value;
{
  return value;
}

static VALUE
rb_mutex_alloc()
{
  Mutex *m;
  m = (Mutex *)malloc(sizeof(Mutex));

  m->owner = Qfalse;
  init_queue(&m->waiting);

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

    put_queue(&m->waiting, current);
    rb_thread_stop();

    rb_thread_critical = Qtrue;
  }
  m->owner = current; 
  rb_thread_critical = Qfalse;
  return self;
}

static VALUE
rb_mutex_unlock_inner(m)
  Mutex *m;
{
  VALUE waking;

  m->owner = Qfalse;

  waking = Qnil;
  while ( m->waiting.entries && !RTEST(waking) ) {
    waking = rb_rescue2(rb_thread_wakeup, get_queue(&m->waiting),
                        return_value, Qnil, rb_eThreadError, 0);
  }

  return waking;
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
  waking = rb_mutex_unlock_inner(m);
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
  rb_mutex_unlock_inner(m);
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
}

