#ifndef MY_PROCESS_H
#define MY_PROCESS_H

#import <pwd.h>
#import <errno.h>
#import <stdio.h>
#import <spawn.h>
#import <stdlib.h>
#import <assert.h>
#import <os/log.h>
#import <stdbool.h>
#import <sys/wait.h>
#import <sys/sysctl.h>
#import <Foundation/Foundation.h>


typedef struct kinfo_proc kinfo_proc;

NS_INLINE
int MyGetBSDProcessList(kinfo_proc * _Nonnull * _Nonnull procList, size_t * _Nonnull procCount)
// Returns a list of all BSD processes on the system.  This routine
// allocates the list and puts it in *procList and a count of the
// number of entries in *procCount.  You are responsible for freeing
// this list (use "free" from System framework).
// On success, the function returns 0.
// On error, the function returns a BSD errno value.
{
    int                 err;
    kinfo_proc *        result;
    bool                done;
    static const int    name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    // Declaring name as const requires us to cast it when passing it to
    // sysctl because the prototype doesn't include the const modifier.
    size_t              length;
    
    assert( procList != NULL);
    assert(*procList == NULL);
    assert(procCount != NULL);
    
    *procCount = 0;
    
    // We start by calling sysctl with result == NULL and length == 0.
    // That will succeed, and set length to the appropriate length.
    // We then allocate a buffer of that size and call sysctl again
    // with that buffer.  If that succeeds, we're done.  If that fails
    // with ENOMEM, we have to throw away our buffer and loop.  Note
    // that the loop causes use to call sysctl with NULL again; this
    // is necessary because the ENOMEM failure case sets length to
    // the amount of data returned, not the amount of data that
    // could have been returned.
    
    result = NULL;
    done = false;
    do {
        assert(result == NULL);
        
        // Call sysctl with a NULL buffer.
        
        length = 0;
        err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1,
                     NULL, &length,
                     NULL, 0);
        if (err == -1) {
            err = errno;
        }
        
        // Allocate an appropriately sized buffer based on the results
        // from the previous call.
        
        if (err == 0) {
            result = (kinfo_proc *)malloc(length);
            if (result == NULL) {
                err = ENOMEM;
            }
        }
        
        // Call sysctl again with the new buffer.  If we get an ENOMEM
        // error, toss away our buffer and start again.
        
        if (err == 0) {
            err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1,
                         result, &length,
                         NULL, 0);
            if (err == -1) {
                err = errno;
            }
            if (err == 0) {
                done = true;
            } else if (err == ENOMEM) {
                assert(result != NULL);
                free(result);
                result = NULL;
                err = 0;
            }
        }
    } while (err == 0 && ! done);
    
    // Clean up and establish post conditions.
    
    if (err != 0 && result != NULL) {
        free(result);
        result = NULL;
    }
    *procList = result;
    if (err == 0) {
        *procCount = length / sizeof(kinfo_proc);
    }
    
    assert((err == 0) == (*procList != NULL));
    
    return err;
}

NS_INLINE
NSDictionary <NSString *, id> * _Nonnull MyProcessDictionaryOfProcess(kinfo_proc * _Nonnull currentProcess)
{
    NSMutableDictionary <NSString *, id> *entry = [NSMutableDictionary dictionaryWithCapacity:6];
    
    NSNumber *processID = [NSNumber numberWithInt:currentProcess->kp_proc.p_pid];
    NSNumber *parentProcessID = [NSNumber numberWithInt:currentProcess->kp_eproc.e_ppid];
    NSNumber *processGroupID = [NSNumber numberWithInt:currentProcess->kp_eproc.e_pgid];
    
    // FIXME: has a maximum length of 16 bytes
    NSString *processName = [NSString stringWithFormat:@"%s", currentProcess->kp_proc.p_comm];
    
    if (processID)
        [entry setObject:processID forKey:@"pid"];
    if (parentProcessID)
        [entry setObject:parentProcessID forKey:@"ppid"];
    if (processGroupID)
        [entry setObject:processGroupID forKey:@"pgid"];
    
    if (processName)
        [entry setObject:processName forKey:@"name"];
    
    struct passwd *user = getpwuid(currentProcess->kp_eproc.e_ucred.cr_uid);
    if (user) {
        NSNumber *userID = [NSNumber numberWithUnsignedInt:currentProcess->kp_eproc.e_ucred.cr_uid];
        NSString *userName = [NSString stringWithFormat:@"%s", user->pw_name];
        
        if (userID)
            [entry setObject:userID forKey:@"uid"];
        if (userName)
            [entry setObject:userName forKey:@"user"];
    }
    
    return [NSDictionary dictionaryWithDictionary:entry];
}

NS_INLINE
NSArray <NSDictionary <NSString *, id> *> * _Nonnull MyGetProcessList(void)
{
    kinfo_proc *myList = NULL;
    size_t myCnt = 0;
    MyGetBSDProcessList(&myList, &myCnt);
    
    NSMutableArray <NSDictionary *> *processes = [NSMutableArray arrayWithCapacity:(int)myCnt];
    for (int i = 0; i < myCnt; i++) {
        struct kinfo_proc *currentProcess = &myList[i];
        [processes addObject:MyProcessDictionaryOfProcess(currentProcess)];
    }
    
    free(myList);
    return [NSArray arrayWithArray:processes];
}

NS_INLINE
int MyProcessByIdentifier(pid_t pid, struct kinfo_proc * _Nonnull proc)
{
    struct kinfo_proc info;
    size_t length = sizeof(struct kinfo_proc);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
    if (sysctl(mib, 4, &info, &length, NULL, 0) < 0 || length == 0)
        return -1;
    memcpy(proc, &info, sizeof(struct kinfo_proc));
    return 0;
}

NS_INLINE
NSDictionary <NSString *, id> * _Nullable MyProcessDictionaryByIdentifier(pid_t identifier)
{
    struct kinfo_proc proc;
    if (MyProcessByIdentifier(identifier, &proc)) {
        return nil;
    }
    return MyProcessDictionaryOfProcess(&proc);
}

NS_INLINE
NSDictionary <NSString *, id> * _Nullable MyProcessDictionaryByName(NSString * _Nonnull name)
{
    kinfo_proc *myList = NULL;
    size_t myCnt = 0;
    MyGetBSDProcessList(&myList, &myCnt);
    
    NSDictionary *targetDict = nil;
    for (int i = 0; i < myCnt; i++) {
        struct kinfo_proc *currentProcess = &myList[i];
        NSDictionary *myDict = MyProcessDictionaryOfProcess(currentProcess);
        if ([myDict[@"name"] isEqualToString:name])
        {
            targetDict = myDict;
            break;
        }
    }
    
    free(myList);
    return targetDict;
}

NS_INLINE
NSString * _Nullable MyProcessExecutablePathByIdentifier(pid_t identifier) {
    
    // First ask the system how big a buffer we should allocate
    int mib[3] = {CTL_KERN, KERN_ARGMAX, 0};
    
    size_t argmaxsize = sizeof(size_t);
    size_t size = 0;
    
    int ret = sysctl(mib, 2, &size, &argmaxsize, NULL, 0);
    if (ret != 0) {
        os_log_error(OS_LOG_DEFAULT, "Error '%{public}s' (%{public}d) getting KERN_ARGMAX", strerror(errno), errno);
        return nil;
    }
    
    // Then we can get the path information we actually want
    mib[1] = KERN_PROCARGS2;
    mib[2] = (int)identifier;
    
    char *procargv = (char *)malloc(size);
    assert(procargv);
    bzero(procargv, size);
    
    ret = sysctl(mib, 3, procargv, &size, NULL, 0);
    
    if (ret != 0) {
        os_log_error(OS_LOG_DEFAULT, "Error '%{public}s' (%{public}d) for pid %{public}d", strerror(errno), errno, identifier);
        free(procargv);
        return nil;
    }
    
    // procargv is actually a data structure.
    // The path is at procargv + sizeof(int)
    NSString *path = [NSString stringWithUTF8String:(procargv + sizeof(int))];
    free(procargv);
    return path;
}

NS_INLINE
void easy_spawn(const char* _Nonnull args[_Nonnull]) {
    pid_t pid;
    int status;
    posix_spawn(&pid, args[0], NULL, NULL, (char* const*)args, NULL);
    waitpid(pid, &status, WEXITED);
}

#endif  /* MY_PROCESS_H */
