/*

  Copyright (c) 2002 Finger Lakes Instrumentation (FLI), L.L.C.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

        Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

        Redistributions in binary form must reproduce the above
        copyright notice, this list of conditions and the following
        disclaimer in the documentation and/or other materials
        provided with the distribution.

        Neither the name of Finger Lakes Instrumentation (FLI), LLC
        nor the names of its contributors may be used to endorse or
        promote products derived from this software without specific
        prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  ======================================================================

  Finger Lakes Instrumentation, L.L.C. (FLI)
  web: http://www.fli-cam.com
  email: support@fli-cam.com

*/

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <glob.h>
#include <pthread.h>

#include "libfli-libfli.h"
#include "libfli-debug.h"
#include "libfli-mem.h"
#include "libfli-camera.h"
#include "libfli-filter-focuser.h"
#include "libfli-sys.h"
#include "libfli-usb.h"

long fli_connect(flidev_t dev, char *name, long domain)
{
  fli_unixio_t *io = NULL;
  pthread_mutex_t mutex;
  pthread_mutexattr_t fliattr;
  fli_unixsysinfo_t *sys;
  int err;

  CHKDEVICE(dev);

  if (name == NULL)
    return -EINVAL;

  /* Lock functions should be set before any other functions used */
  DEVICE->fli_lock = fli_lock;
  DEVICE->fli_unlock = fli_unlock;
  DEVICE->fli_trylock = fli_trylock;
  
  DEVICE->domain = domain & 0x00ff;
  DEVICE->devinfo.type = domain & 0xff00;

  debug(FLIDEBUG_INFO, "Domain: 0x%04x", DEVICE->domain);
  debug(FLIDEBUG_INFO, "  Type: 0x%04x", DEVICE->devinfo.type);

  switch (DEVICE->devinfo.type)
  {
  case FLIDEVICE_CAMERA:
    DEVICE->fli_open = fli_camera_open;
    DEVICE->fli_close = fli_camera_close;
    DEVICE->fli_command = fli_camera_command;
    break;

  case FLIDEVICE_FOCUSER:
    DEVICE->fli_open = fli_focuser_open;
    DEVICE->fli_close = fli_focuser_close;
    DEVICE->fli_command = fli_focuser_command;
    break;

  case FLIDEVICE_FILTERWHEEL:
    DEVICE->fli_open = fli_filter_open;
    DEVICE->fli_close = fli_filter_close;
    DEVICE->fli_command = fli_filter_command;
    break;

  default:
    return -EINVAL;
  }

  if ((io = xcalloc(1, sizeof(fli_unixio_t))) == NULL)
    return -ENOMEM;
    
  io->fd = (-1); /* No device open at this time */
  io->han = NULL;

  switch (DEVICE->domain)
  {
  case FLIDOMAIN_USB:
    {
      int r;

      if ((r = libusb_usb_connect(dev, io, name)))
      {
		libusb_usb_disconnect(dev,io);
        xfree(io);
        return r;
      }
      
      switch(DEVICE->devinfo.type)
      {
        case FLIDEVICE_CAMERA:
          if (!((DEVICE->devinfo.devid == FLIUSB_CAM_ID) ||
          	(DEVICE->devinfo.devid == FLIUSB_PROLINE_ID)))
          {
			libusb_usb_disconnect(dev,io);
            xfree(io);
            return -ENODEV;
          }
          break;

        case FLIDEVICE_FOCUSER:
          if (DEVICE->devinfo.devid != FLIUSB_FOCUSER_ID)
          {
			libusb_usb_disconnect(dev,io);
            xfree(io);
            return -ENODEV;
          }
          break;

        case FLIDEVICE_FILTERWHEEL:
          if (!((DEVICE->devinfo.devid == FLIUSB_FILTER_ID) ||
          	(DEVICE->devinfo.devid == FLIUSB_CFW4_ID)))
          {
            debug(FLIDEBUG_INFO, "FW Not Recognized");
			libusb_usb_disconnect(dev,io);
            xfree(io);
            return -ENODEV;
          }
          break;

        default:
          debug(FLIDEBUG_INFO, "Device Not Recognized");
		  libusb_usb_disconnect(dev,io);
          xfree(io);
          return -ENODEV;
      }
      
      DEVICE->fli_io = libusb_usbio;
    }
    break;

  default:
    xfree(io);
    return -EINVAL;
  }

  if((sys = xcalloc(1, sizeof(fli_unixsysinfo_t))) == NULL)
  {
    fli_disconnect(dev);
    return -ENOMEM;
  }
  DEVICE->sys_data = sys;

  /* Create synchronization object */
  if((err = pthread_mutexattr_init(&fliattr)) != 0)
    return err;
  
  if((err = pthread_mutexattr_settype(&fliattr, PTHREAD_MUTEX_NORMAL)) != 0)
    return err;
  
  if((err = pthread_mutexattr_setpshared(&fliattr, PTHREAD_PROCESS_SHARED)) != 0)
    return err;
  
  if((err = pthread_mutex_init(&mutex,&fliattr)) != 0)
    return err;

  ((fli_unixsysinfo_t *) (DEVICE->sys_data))->mutex = mutex;
  ((fli_unixsysinfo_t *) (DEVICE->sys_data))->attr = fliattr;
  
  DEVICE->io_data = io;
  DEVICE->name = xstrdup(name);
  DEVICE->io_timeout = 60 * 1000; /* 1 min. */

  debug(FLIDEBUG_INFO, "Connected");

  return 0;
}

long fli_disconnect(flidev_t dev)
{
  int err = 0;
  fli_unixio_t *io;
  fli_unixsysinfo_t *sys;

  CHKDEVICE(dev);

  if ((io = DEVICE->io_data) == NULL)
    return -EINVAL;

  if ((sys = DEVICE->sys_data) == NULL)
    return -EINVAL;

  err = pthread_mutex_destroy(&(sys->mutex));
  err = pthread_mutexattr_destroy(&(sys->attr));

  switch (DEVICE->domain)
  {
  case FLIDOMAIN_USB:
    err = libusb_usb_disconnect(dev,io);
    break;

  default:
  	err = close(io->fd);
    break;
  }

  if (err)
    err = -errno;

  xfree(DEVICE->io_data);
  DEVICE->io_data = NULL;

  if(DEVICE->sys_data != NULL)
  {
    xfree(DEVICE->sys_data);
    DEVICE->sys_data = NULL;
  }

  DEVICE->fli_lock = NULL;
  DEVICE->fli_unlock = NULL;
  DEVICE->fli_io = NULL;
  DEVICE->fli_open = NULL;
  DEVICE->fli_close = NULL;
  DEVICE->fli_command = NULL;

  return err;
}

long fli_lock(flidev_t dev)
{
  long r = -ENODEV;
  fli_unixsysinfo_t *sys;
  pthread_mutex_t mutex;

  CHKDEVICE(dev);

  if ((sys = DEVICE->sys_data) == NULL)
  {
    debug(FLIDEBUG_WARN, "lock(): Mutex is NULL!");
    return r;
  }

  mutex = sys->mutex;

  r = pthread_mutex_lock(&mutex);
  if(r != 0)
  {
    debug(FLIDEBUG_WARN, "Could not acquire mutex: %d", r);
    r = -ENODEV;
  }

  return r;
}

long fli_unlock(flidev_t dev)
{
  int r = -ENODEV;
  fli_unixsysinfo_t *sys;
  pthread_mutex_t mutex;

  CHKDEVICE(dev);
  
  sys = DEVICE->sys_data;

  if (sys == NULL)
  {
    debug(FLIDEBUG_WARN, "unlock(): Mutex is NULL!");
    return r;
  }

  mutex = sys->mutex;

  r = pthread_mutex_unlock(&mutex);
  if( r != 0)
  {
    debug(FLIDEBUG_WARN, "Could not release mutex: %d", r);
    return -ENODEV;
  }

  return r;
}

long fli_trylock(flidev_t dev)
{
  long r = -ENODEV;
  fli_unixsysinfo_t *sys;
  pthread_mutex_t mutex;

  CHKDEVICE(dev);

  if ((sys = DEVICE->sys_data) == NULL)
  {
    debug(FLIDEBUG_WARN, "trylock(): Mutex is NULL!");
    return r;
  }

  mutex = sys->mutex;

  r = pthread_mutex_trylock(&mutex);
  if(r != 0)
  {
    debug(FLIDEBUG_WARN, "Could not acquire mutex with trylock: %d", r);
    r = -ENODEV;
  }

  return r;
}

long fli_list(flidomain_t domain, char ***names)
{
  *names = NULL;

  switch (domain & 0x00ff)
  {
  case FLIDOMAIN_USB:
    return libusb_list(domain, names);
    break;

  default:
    return -EINVAL;
  }

  /* Not reached */
  return -EINVAL;
}