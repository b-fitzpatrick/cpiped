/*
 * Original cpiped.c code Copyright (C) 2014 Brian Fitzpatrick <brian@froxen.com>
 * 
 * Modifications 2020 Nate Dreger <info@natedreger.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE

// Use the newer ALSA API
#define ALSA_PCM_NEW_HW_PARAMS_API

#include <stdio.h>
#include <math.h>
#include <alsa/asoundlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>

snd_pcm_t *handle;
int daemonize = 0;
int readfd = 0;
int writefd = 0;

void mylog(int level, const char *format, ...)
{
    va_list args;
    va_start (args, format);

    if (daemonize) {
        vsyslog(level, format, args);
    } else {
    vprintf(format, args);
    }
    va_end(args);
}

void myterm() {
  mylog(LOG_INFO, "Stopping due to interrupt/term.\n");
  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  if (readfd > 0) close(readfd);
  if (writefd > 0) close(writefd);
  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
  int rc;
  int capsize;
  snd_pcm_hw_params_t *params;
  unsigned int val = 44100;
  unsigned int capusec;
  int dir;
  snd_pcm_uframes_t frames;
  char *capbuffer;
  int16_t *scapbuffer;
  char *capdev = "default";
  char *fifonam;
  struct stat status;
  int readbytes = 0;
  int writebytes = 0;
  char *buf;
  size_t bufsize = 176400;
  int bufstart;
  int bufend;
  int bufused;
  int fillbuf = 1;
  int buffull = 0;
  int bufendtoend;
  int bufstarttoend;
  int capcount = 0;
  long power = 0;
  int prevpower = 0;
  int i;
  int silent = 1;
  int silentcount = 0;
  int soundcount = 0;
  char startcmd[1000] = "";
  char endcmd[1000] = "";
  int wrote = 0;
  int silentt = 100;
  
  extern char *optarg;
  extern int optind;
  int opt;
  int err = 0;
  
  pid_t pid, sid;
  char pidstr[10];
  char pidfile[50];
  int pidfd;

  // Ignore pipe signals
  signal(SIGPIPE, SIG_IGN);
  // Flush the hardware buffer on interrupt/term
  signal(SIGINT, myterm);
  signal(SIGTERM, myterm);

  // Process command-line options and arguments
  while ((opt = getopt(argc, argv, "d:b:s:e:t:D")) != -1) {
    switch (opt) {
    case 'd':
      capdev = optarg;
      break;
    case 'b':
      bufsize = atof(optarg) * val * 8;
      if (bufsize < 33280 || bufsize > 1764000) {
        mylog(LOG_ERR, "Invalid buffer. Range is 0.1-5.0 sec.\n");
        goto error;
      }
      break;
    case 's':
      strcpy(startcmd, optarg);
      if (stat(startcmd, &status) != 0) {
        mylog(LOG_ERR, "Invalid start command: %s\n", startcmd);
        goto error;
      }
      strcat(startcmd, " &");
      break;
    case 'e':
      strcpy(endcmd, optarg);
      if (stat(endcmd, &status) != 0) {
        mylog(LOG_ERR, "Invalid end command: %s\n", endcmd);
        goto error;
      }
      strcat(endcmd, " &");
      break;
    case 't':
      silentt = atoi(optarg);
      if ((silentt < 1) || (silentt > 32767)) {
        mylog(LOG_ERR, "Invalid silence threshold. Range is 1-32767.\n");
        goto error;
      }
      break;
    case 'D':
      daemonize = 1;
      break;
    case '?':
      err = 1;
      mylog(LOG_ERR, "Invalid option: %s\n", argv[optind - 1]);
      break;
    }
  }
  if ((optind + 1) != argc) err = 1;
  if (err) {
    if (!daemonize) {
      printf("\nUsage:\n %s [-d arg] [-b arg] [-s arg] "
      "[-e arg] [-t arg] [-D] FIFO\n"
      " -d : ALSA capture device ['default']\n"
      " -b : target buffer in seconds [.5]\n"
      " -s : command to run when sound detected\n"
      " -e : command to run when silence detected\n"
      " -t : silence threshold (1 - 32767, [100])\n"
      " -D : daemonize\n"
      " FIFO : path to a named pipe\n", argv[0]);
    }
    goto error;
  }
  
  mylog(LOG_INFO, "Starting up.\n");
  
  fifonam = argv[argc - 1];

  // Daemonize
  if (daemonize) {
    pid = fork();
    if (pid < 0) {
      exit(EXIT_FAILURE);
    } else if (pid > 0) {
      exit(EXIT_SUCCESS);
    }
    
    umask(0);
    
    sid = setsid();
    if (sid < 0) {
      mylog(LOG_ERR, "Error creating new session: %s\n", strerror(errno));
      goto error;
    }
    
    if (chdir("/") != 0) {
      mylog(LOG_ERR, "Error on chdir /: %s\n", strerror(errno));
      goto error;
    }
  }

  // Write the pidfile
  sprintf(pidfile, "/var/run/cpiped.pid");
  sprintf(pidstr, "%d\n", getpid());
  pidfd = open(pidfile, O_RDWR|O_CREAT, 0644);
  write(pidfd, pidstr, strlen(pidstr));
  close(pidfd);
  
  // Open the FIFO for read first, so the open for write succeeds.
  readfd = open(fifonam, O_RDONLY | O_NONBLOCK);
  if (readfd <= 0) {
    mylog(LOG_ERR, "Error opening FIFO for read: %s\n", strerror(errno));
    goto error;
  }
  if (fstat(readfd, &status) != 0) {
    mylog(LOG_ERR, "fstat error on FIFO (%s).\n", fifonam);
    goto error;
  }
  if (!S_ISFIFO(status.st_mode)) {
    mylog(LOG_ERR, "%s is not a FIFO.\n", fifonam);
    goto error;
  }
  writefd = open(fifonam, O_WRONLY);
  if (writefd <= 0) {
    mylog(LOG_ERR, "Error opening FIFO for write: %s\n", strerror(errno));
    goto error;
  }
  close(readfd);
  
  // Set the FIFO size to 8192 bytes to minimize latency
  fcntl(writefd, F_SETPIPE_SZ, 8192);
  
  // Open PCM device for recording (capture).
  rc = snd_pcm_open(&handle, capdev, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
  if (rc != 0) {
    mylog(LOG_ERR, "Unable to open pcm device: %s\n", snd_strerror(rc));
    goto error;
  }
  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(handle, params);
  snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(handle, params, 2);
  snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

  // Set period size to 1024 frames.
  frames = 1024;
  snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

  // Write the parameters to the driver
  rc = snd_pcm_hw_params(handle, params);
  if (rc != 0) {
    mylog(LOG_ERR, "Unable to set hw parameters: %s\n", snd_strerror(rc));
    goto error;
  }

  // Determine the elapsed time of one period
  snd_pcm_hw_params_get_period_time(params, &capusec, &dir);
  
  // Create a capture buffer large enough to hold one period
  snd_pcm_hw_params_get_period_size(params, &frames, &dir);
  capsize = frames * 4; // 2 bytes/sample, 2 channels
  capbuffer = calloc(capsize, sizeof(char));
  scapbuffer = (int16_t*)capbuffer;
  
  // Determine write size
  writebytes = capsize - 32 ; // Write a bit less to make sure write controls pace

  // Create a ring buffer to handle clock mismatch
  bufsize++; // Add one byte to permit determination of empty/full states
  buf = calloc(bufsize, sizeof(char));
  bufstart = 0;
  bufend = 0;
  
  while (1) { 
    // Capture samples
    if (!wrote) // When not writing, wait a bit between captures.
      usleep(capusec * .95);
    rc = snd_pcm_readi(handle, capbuffer, frames);
    if (rc == -EPIPE) { // EPIPE means overrun
      mylog(LOG_NOTICE, "Overrun occurred\n");
      snd_pcm_prepare(handle);
    } else if (rc == -EAGAIN) {
      rc = 0; // Ignore "Not Available" errors
    } else if (rc < 0) {
      mylog(LOG_ERR, "Capture error: %s\n", snd_strerror(rc));
    }
 
    // Check if there is sound and if so compute RMS power level regularly and count sounds and silences
    if (capcount > 10 && rc > 0) { // Approx. every quarter second
      power = 0;
      for (i = 0; i < rc / 2; i++) {
        power += pow(scapbuffer[i], 2) + pow(scapbuffer[i + 1], 2);
      }
      power = sqrt(power/rc);
      capcount = 0;
      
      if (power > silentt) {
        soundcount++;
        silentcount = 0;
      } else {
        silentcount++;
        soundcount = 0;
      }
      
      if (silent && soundcount > 1) {
        silent = 0;
        mylog(LOG_INFO, "Sound detected. Last two values: %d, %d\n", prevpower, (int)power);
        if (strcmp(startcmd, "") != 0) {
          mylog(LOG_INFO, "Running '%s'\n", startcmd);
          if (system(startcmd) != 0) {
            mylog(LOG_ERR, "Error running start command.\n");
          }
        }
      }
      
      if (!silent && silentcount > 55) {
        silent = 1;
        mylog(LOG_INFO, "Silence detected. Last two values: %d, %d\n", prevpower, (int)power);
        if (strcmp(endcmd, "") != 0) {
          mylog(LOG_INFO, "Running '%s'\n", endcmd);
          if (system(endcmd) != 0) {
            mylog(LOG_ERR, "Error running end command.\n");
          }
        }
      }
    }
    prevpower = power;
    capcount++;
  // end of soundcounter   
  
  // Check if there is sound, if so write to the pipe
  if (soundcount > 0) { 

    // Determine the amount of buffer used
    bufused = bufend - bufstart + (bufend < bufstart) * (bufsize - 1);
    //printf("size:%d, start:%d, end:%d, used:%d, wrote:%d\n", bufsize, bufstart, bufend, bufused, wrote);
    
    if (bufused < capsize) {
      // Buffer is almost empty, don't send to pipe
      mylog(LOG_INFO, "Filling buffer\n");
      // nate code
      mylog(LOG_INFO, "Last two values: %d, %d\n", prevpower, (int)power);
      fillbuf = 1;
    }
    
    if (bufused > (int)bufsize - 1 - capsize) {
      // Buffer is almost full, don't store captured samples
      mylog(LOG_INFO, "Buffer full\n");
      buffull = 1;
    }

    // Resume both capture storage and write to pipe when the buffer reaches half-full
    if ((fillbuf == 1) && (bufused > (int)bufsize / 2))
      fillbuf = 0;
    if ((buffull == 1) && (bufused < (int)bufsize / 2))
      buffull = 0;
    
    
    if (!buffull  && rc > 0) {	
      // Store samples in buffer
      readbytes = rc * 4;
      bufendtoend = bufsize - bufend;
      if (readbytes <= bufendtoend) {
        // No wrap required
        memcpy(buf + bufend, capbuffer, readbytes);
      } else {
        // Wrap required
        memcpy(buf + bufend, capbuffer, bufendtoend);
        memcpy(buf, capbuffer + bufendtoend, readbytes - bufendtoend);
      }
      bufend = (bufend + readbytes) % bufsize;
    }
   
    wrote = 0;
    if (!fillbuf) {
      bufstarttoend = bufsize - bufstart;
      if (writebytes <= bufstarttoend) { // No wrap required
        rc = write(writefd, buf + bufstart, writebytes);
        if (rc == writebytes) wrote = 1;
      } else { // Wrap required
        rc = write(writefd, buf + bufstart, bufstarttoend);
        rc = write(writefd, buf, writebytes - bufstarttoend);
        if (rc == writebytes - bufstarttoend) wrote = 1;
      }
      if (wrote) {
        bufstart = (bufstart + writebytes) % bufsize;
      } else {
        bufstart = (bufstart + readbytes) % bufsize; // Keep buffer used constant
      }
    }
  } // end if
 }
  exit(EXIT_SUCCESS);
  
error:
  if (readfd > 0) close(readfd);
  if (writefd > 0) close(writefd);
  mylog(LOG_INFO, "Stopping due to error.");
  exit(EXIT_FAILURE);
}
