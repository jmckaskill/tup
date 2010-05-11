/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#include "lock.h"
#include "config.h"
#include <stdio.h>
#include <time.h>

/* So...the tri-lock business. There are three locks. They lock one thing - the
 * database. It also lets the monitor ignore the file accesses while an update
 * happens. I think they may actually all be necessary. Here's how it works,
 * more or less:
 *
 * If we're not running the monitor, everything is pretty simple - everybody
 * waits on the shared lock until whoever has that lock is done with it. They
 * keep the shared lock for their whole operation, so they can do whatever they
 * want with the database. It's called the shared lock because it used to be
 * actually shared (as in, read-only). However that makes things confusing. Now
 * it's just a mutex to make sure only one process opens/closes the object lock
 * file at a time. In theory it should be renamed to only_one_dude_can_use_the_
 * object_lock_file_at_a_time_lock, but that would be stupid.
 *
 * Now if we run the monitor, it comes in here and eventually gets the shared
 * and object locks, so it is king of the universe. The monitor, as part of its
 * initialization, will put a watch on the object lock file. It will then
 * remove its shared lock. This means anyone else who was waiting on the shared
 * lock can go and open the object lock file, which notifies the monitor. Note
 * if we didn't have the shared lock, we could have two processes open the
 * object lock and block on the flock. Then when the monitor tries to get the
 * lock again, it may succeed, but not know about the second process because
 * the file open already happened. One reason this may happen is inotify can
 * slurp duplicate events (two opens on the same file by different processes).
 *
 * So the monitor got the file notification on the object lock, it will flush
 * its queue and then take the tri lock, and release the object lock. The only
 * process waiting on the object lock is the one who has the shared lock, so he
 * gets to go. When he finishes he will release the object lock and close it.
 * The file close notification informs the monitor that it should take the
 * object lock again. Once it has the lock, the monitor will remove the tri
 * lock, letting the other process finish its unlocking. The other process then
 * drops the shared lock, so the next guy can go and open the object lock (and
 * inform the monitor again). Essentially the tri lock gives the monitor the
 * highest priority in obtaining the object lock once it is available again.
 * This way we can be sure we don't miss any events in between two subsequent
 * processes.
 *
 * With the locks as they are, the monitor doesn't drop events in between other
 * processes trying to go, and also makes sure it doesn't miss someone trying
 * to get the object lock. I ran t7005 in a loop like a million times to verify
 * it. I suppose a formal proof would be nice, but I say meh! to you. With
 * different locking mechanisms, I would sometimes get a deletion event from
 * the updater, but the monitor would read it as if it were outside the updater
 * and also try to delete the file. Another issue would be deadlocking from the
 * monitor not detecting someone trying to use the object lock. With three
 * locks I don't get those issues.
 */

static fd_t sh_lock;
static fd_t obj_lock;
static fd_t tri_lock;

int tup_lock_init(void)
{
	if(fd_openat(tup_top_fd(), TUP_SHARED_LOCK, O_RDWR, &sh_lock)) {
		perror(TUP_SHARED_LOCK);
		return -1;
	}
	if(fd_lock(sh_lock) < 0) {
		return -1;
	}

	if(fd_openat(tup_top_fd(), TUP_OBJECT_LOCK, O_RDWR, &obj_lock)) {
		perror(TUP_OBJECT_LOCK);
		return -1;
	}
	if(fd_lock(obj_lock) < 0) {
		return -1;
	}

	if(fd_openat(tup_top_fd(), TUP_TRI_LOCK, O_RDWR, &tri_lock)) {
		perror(TUP_TRI_LOCK);
		return -1;
	}
	return 0;
}

void tup_lock_exit(void)
{
	fd_unlock(obj_lock);
	fd_close(obj_lock);
	/* Wait for the monitor to pick up the object lock */
	fd_lock(tri_lock);
	fd_unlock(tri_lock);
	fd_close(tri_lock);
	fd_unlock(sh_lock);
	fd_close(sh_lock);
}

void tup_lock_close(void)
{
	fd_close(obj_lock);
	fd_close(tri_lock);
	fd_close(sh_lock);
}

fd_t tup_sh_lock(void)
{
	return sh_lock;
}

fd_t tup_obj_lock(void)
{
	return obj_lock;
}

fd_t tup_tri_lock(void)
{
	return tri_lock;
}

