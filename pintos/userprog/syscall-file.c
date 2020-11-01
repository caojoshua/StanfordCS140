#include "userprog/syscall-file.h"
#include <debug.h>
#include <stdbool.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

/* Min fd. fd 0 and fd 1 are reserved for stdin and stdout
 * respectively. */
static const int MIN_FD = 2;

/***** File Descriptor *****/

/* Create and return a new file descriptor. */
int
create_fd (const char *file_name)
{
  if (!file_name)
    return -1;
  struct file *file = filesys_open (file_name);
  if (!file)
    return -1;

  struct process *process = thread_current ()->process;
  if (process)
  {
    int fd = MIN_FD;
    struct list *fd_map = &process->fd_map;

    struct file_descriptor *file_descriptor = malloc (sizeof (struct file_descriptor));
    file_descriptor->file = file;

    /* Iterate through the fd list until there is an open fd */
    struct list_elem *e;
    for (e = list_begin (fd_map); e != list_end (fd_map); e = list_next (e))
    {
      struct file_descriptor *fd_temp = list_entry (e, struct file_descriptor, elem);
      if (fd != fd_temp->fd)
      {
        file_descriptor->fd = fd;
        e = list_next(e);
        break;
      }
      ++fd;
    }
    file_descriptor->fd = fd;
    list_insert (e, &file_descriptor->elem);
    return fd;
  }
  return -1;
}

/* Cleans FDs with given pid */
void
clean_fds (pid_t pid)
{
  struct process *process = thread_current ()->process;
  if (process)
  {
    struct list *fd_map = &process->fd_map;
    struct list_elem *e;
    for (e = list_begin (fd_map); e != list_end (fd_map); e = list_next (e))
    {
      struct file_descriptor *file_descriptor = list_entry (e, struct file_descriptor, elem);
      if (process->pid == pid)
      {
        struct list_elem *temp = list_prev (e);
        list_remove (e);
        e = temp;
        file_close (file_descriptor->file);
        free (file_descriptor);
      }
    }
  }
}

/* Returns the file associated with the given fd */
struct file_descriptor*
get_file_descriptor (int fd)
{
  struct process *process = thread_current ()->process;
  if (process)
  {
    struct list *fd_map = &process->fd_map;
    struct list_elem *e;
    for (e = list_begin (fd_map); e != list_end (fd_map); e = list_next (e))
    {
      struct file_descriptor *file_descriptor = list_entry (e, struct file_descriptor, elem); 
      if (file_descriptor->fd == fd)
        return file_descriptor;
    }
  }
  return NULL;
}


/***** Map ID *****/

/* HASH function for mapid_entry hash. */
unsigned
mapid_hash (const struct hash_elem *m_, void *aux UNUSED)
{
    const struct mapid_entry *m = hash_entry (m_, struct mapid_entry, hash_elem);
    return hash_bytes (&m->mapid, sizeof m->mapid);
}

/* LESS function for mapid_entry hash. */
bool
mapid_less (const struct hash_elem *a_, const struct hash_elem *b_,
    UNUSED void* aux)
{
  struct mapid_entry *a = hash_entry (a_, struct mapid_entry, hash_elem);
  struct mapid_entry *b = hash_entry (b_, struct mapid_entry, hash_elem);
  return a->mapid < b->mapid;
}

/* Returns a pointer to the mapid_entry with given mapid, or NULL if none
 * exists. */
static struct mapid_entry*
mapid_lookup (int mapid)
{
	struct mapid_entry m;
  m.mapid = mapid;
  struct hash_elem *e;

	struct process *cur = thread_current ()->process;
	if (cur)
	{
		e = hash_find (&cur->mapid_map, &m.hash_elem);
		if (e)
			return hash_entry (e, struct mapid_entry, hash_elem);
	}
	return NULL;
}

/* Create a new mapid_entry mapping pages to file open in fd. Returns a pointer
 * to the mapid_entry, or NULL if there is a failure. */
struct mapid_entry*
create_mapid (int fd, void* addr)
{
  ASSERT (is_user_vaddr (addr));

  struct process *process = thread_current ()->process;
  struct file_descriptor *file_descriptor = get_file_descriptor (fd);
  if (process && file_descriptor)
  {
    /* Find an available mapid. */
    for (int i = 0; true; ++i)
    {
      if (!mapid_lookup (i))
      {
        struct mapid_entry *mapid = malloc (sizeof (struct mapid_entry));
        mapid->mapid = i;
        mapid->file = file_descriptor->file;
        mapid->addr = addr;
        mapid->length = file_length (mapid->file);
        hash_insert (&process->mapid_map, &mapid->hash_elem);
        return mapid;
      }
    }
  }
  return NULL;
}

/* Removes a mapid from the current processes mapid_table. Write the mapped
 * pages to the file and free the mapid_entry. */
void
remove_mapid (int mapid)
{
  struct process *process = thread_current ()->process;
  struct mapid_entry *mapid_entry = mapid_lookup (mapid);
  if (mapid_entry)
  {
    /* Write the mmap page contents to kernel space first, then write it to
     * to disk. This is to avoid page faults, which must not occure during
     * filesys access. Page faults can occur if a mmap page is not in memory.
     * TODO: find a better way to avoid page faults without doing a full
     * malloc/memcpy. This way is costly, and doesn't fully ensure that the
     * contents will be in memory. */
    void *file_contents = malloc (mapid_entry->length);
    memcpy (file_contents, mapid_entry->addr, mapid_entry->length);
    int size = file_write_at (mapid_entry->file, file_contents, mapid_entry->length, 0);
    free (file_contents);

    /* No requirement listed to set the file position back to 0, but test
     * cases imply so. */
    file_seek (mapid_entry->file, 0);

    /* Free pages. */
    void *addr = pg_round_down (mapid_entry->addr);
    void *end_addr = mapid_entry->addr + mapid_entry->length;
    while (addr <= end_addr)
    {
      page_free (addr);
      addr += PGSIZE;
    }

    hash_delete (&process->mapid_map, &mapid_entry->hash_elem);
    free (mapid_entry);
  }
}
