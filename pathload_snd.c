/*
 This file is part of pathload.

 pathload is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 pathload is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with pathload; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*-------------------------------------------------
   pathload : an end-to-end available bandwidth 
              estimation tool
   Author   : Manish Jain (jain@cc.gatech.edu)
              Constantinos Dovrolis (dovrolis@cc.gatech.edu)
   Release  : Ver 1.3.2
   Support  : This work was supported by the SciDAC
              program of the US department 
--------------------------------------------------*/
#define LOCAL
#define MAX_SOCK 20
#include "pathload_gbls.h"
#include "pathload_snd.h"

int main(int argc, char* argv[])
{
  struct hostent *host_rcv;
  struct timeval tv1,tv2;
  l_uint32 snd_time ;
  l_int32 ctr_code ;
  time_t localtm;
  int opt_len,mss;
  int ret_val ;
  int iterate=0;
  int done=0;
  int latency[30],ord_latency[30];
  int i;
  int c ;
  int errflg=0;
  char pkt_buf[256];
  char ctr_buff[8];

  /* Additional var for IPv6 suppport */
  struct addrinfo hints, *res, *res0;
  int error;
  struct sockaddr_storage from;
  socklen_t fromlen;
  int ls;
  int s[MAX_SOCK];
  int smax;
  int sockmax;
  fd_set rfd, rfd0;
  int n;
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  const int on = 1;

  quiet=0;
  while ((c = getopt(argc, argv, "ihHq")) != EOF)
    switch (c) 
    {
      case 'H':
      case 'h':
        help() ;
        break ;
      case 'i':
        iterate=1;
        break;
      case 'q':
        quiet=1;
        break;
      case '?':
        errflg++;
    }
  if (errflg)
  {
    fprintf(stderr, "usage: pathload_snd [-q] [-H|-h]\n");
    exit(-1);
  }

  num_stream = NUM_STREAM ;
  min_sleeptime();

  /* gettimeofday latency */
  for(i=0;i<30;i++)
  {
    gettimeofday(&tv1,NULL);
    gettimeofday(&tv2,NULL);
    latency[i]=tv2.tv_sec*1000000+tv2.tv_usec-tv1.tv_sec*1000000-tv1.tv_usec;
  }
  order_int(latency,ord_latency,30);
  gettimeofday_latency = ord_latency[15];  
#ifdef DEBUG
  printf("DEBUG :: gettimeofday_latency = %d\n",gettimeofday_latency);
#endif


/* gettimeofday latency */
  for (i = 0; i < 30; i++)
  {
    gettimeofday(&tv1, NULL);
    gettimeofday(&tv2, NULL);
    latency[i] = tv2.tv_sec * 1000000 + tv2.tv_usec - tv1.tv_sec * 1000000 - tv1.tv_usec;
  }
  order_int(latency, ord_latency, 30);
  gettimeofday_latency = ord_latency[15];
#ifdef DEBUG
  printf("DEBUG :: gettimeofday_latency = %d\n", gettimeofday_latency);
#endif

  /* Additional process for IPv6 support */
  snprintf(sbuf, sizeof(sbuf), "%u", TCPSND_PORT);
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  error = getaddrinfo(NULL, sbuf, &hints, &res0);
  if (error)
  {
    fprintf(stderr, "%s: %s\n", sbuf, gai_strerror(error));
    exit(EXIT_FAILURE);
  }
  smax = 0;
  sockmax = -1;
  /* Create TCP sockets for listening for connections */
  for (res = res0; res && smax < MAX_SOCK; res = res->ai_next)
  {
    s[smax] = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s[smax] < 0)
    {
      continue;
    }

    if (s[smax] >= FD_SETSIZE)
    {
      close(s[smax]);
      s[smax] = -1;
      continue;
    }

    if (res->ai_family == AF_INET6 &&
        setsockopt(s[smax], IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0)
    {
      perror("setsockopt");
      s[smax] = -1;
      continue;
    }

    if (bind(s[smax], res->ai_addr, res->ai_addrlen) < 0)
    {
      close(s[smax]);
      s[smax] = -1;
      continue;
    }

    if (listen(s[smax], 5) < 0)
    {
      close(s[smax]);
      s[smax] = -1;
      continue;
    }

    error = getnameinfo(res->ai_addr, res->ai_addrlen,
                        hbuf, sizeof(hbuf), sbuf, sizeof(sbuf),
                        NI_NUMERICHOST | NI_NUMERICSERV);
    if (error)
    {
      fprintf(stderr, "./server: %s\n", gai_strerror(error));
      exit(EXIT_FAILURE);
    }
    fprintf(stderr, "listen to %s %s\n", hbuf, sbuf);

    if (s[smax] > sockmax)
    {
      sockmax = s[smax];
    }
    smax++;
  }
  if (smax == 0)
  {
    fprintf(stderr, "No socket to listen to\n");
    exit(EXIT_FAILURE);
  }
  freeaddrinfo(res0);


  FD_ZERO(&rfd0);
  for (i = 0; i < smax; i++)
  {
    FD_SET(s[i], &rfd0);
  }
  do
  {
    if (!quiet)
      printf("\n\nWaiting for receiver to establish control stream => ");
    fflush(stdout);
    rfd = rfd0;
    n = select(sockmax + 1, &rfd, NULL, NULL, NULL);
    if (n < 0)
    {
      perror("select");
      exit(EXIT_FAILURE);
    }
    for (i = 0; i < smax; i++)
    {
      if (FD_ISSET(s[i], &rfd))
      {
        rcv_tcp_adrlen = sizeof(rcv_tcp_addr);
        sock_tcp = s[i];
        ctr_strm = accept(sock_tcp, (struct sockaddr *)&rcv_tcp_addr, &rcv_tcp_adrlen);
        if (ctr_strm < 0)
        {
          perror("accept(sock_tcp)");
          continue;
        }
        if (!quiet)
          printf("OK\n");
        localtm = time(NULL);

        error = getnameinfo((struct sockaddr *)&rcv_tcp_addr, &rcv_tcp_adrlen,
                            hbuf, sizeof(hbuf), sbuf, sizeof(sbuf),
                            NI_NUMERICHOST | NI_NUMERICSERV);
        if (error)
        {
          fprintf(stderr, "./server: %s\n", gai_strerror(error));
          exit(EXIT_FAILURE);
        }
        fprintf(stderr, "connection from: %s %s\n", hbuf, sbuf);

        /* Create UDP socket for going connect to */
        snprintf(sbuf, sizeof(sbuf), "%u", UDPRCV_PORT);
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_DGRAM;
        error = getaddrinfo(hbuf, sbuf, &hints, &res0);
        if (error)
        {
          fprintf(stderr, "%s %s: %s\n", hbuf, sbuf, gai_strerror(error));
          exit(EXIT_FAILURE);
        }
        for (res = res0; res; res = res->ai_next)
        {
          error = getnameinfo(res->ai_addr, res->ai_addrlen,
                              hbuf, sizeof(hbuf), sbuf, sizeof(sbuf),
                              NI_NUMERICHOST | NI_NUMERICSERV);
          if (error)
          {
            fprintf(stderr, "%s %s: %s\n", hbuf, sbuf, gai_strerror(error));
            continue;
          }
          fprintf(stderr, "trying %s port %s\n", hbuf, sbuf);

          sock_udp = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
          if (sock_udp < 0)
          {
            continue;
          }
          if (connect(sock_udp, res->ai_addr, res->ai_addrlen) < 0)
          {
            close(sock_udp);
            sock_udp = -1;
            continue;
          }

          /* Make TCP socket non-blocking */
          if (fcntl(ctr_strm, F_SETFL, O_NONBLOCK) < 0)
          {
            perror("fcntl(ctr_strm, F_SETFL, O_NONBLOCK):");
            exit(-1);
          }
          opt_len = sizeof(mss);
          if (getsockopt(ctr_strm, IPPROTO_TCP, TCP_MAXSEG, (char *)&mss, &opt_len) < 0)
          {
            perror("getsockopt(sock_tcp,IPPROTO_TCP,TCP_MAXSEG):");
            exit(-1);
          }
          snd_max_pkt_sz = mss;
          if (snd_max_pkt_sz == 0 || snd_max_pkt_sz == 1448)
            snd_max_pkt_sz = 1472; /* Make it Ethernet sized MTU */
          else
            snd_max_pkt_sz = mss + 12;

          /* tell receiver our max packet sz */
          send_ctr_mesg(ctr_buff, snd_max_pkt_sz) ;
          /* receiver's maxp packet size */
          while ((rcv_max_pkt_sz = recv_ctr_mesg( ctr_buff)) == -1);
          max_pkt_sz = (rcv_max_pkt_sz < snd_max_pkt_sz) ? rcv_max_pkt_sz:snd_max_pkt_sz ;
          if ( !quiet )
            printf("Maximum packet size          :: %ld bytes\n",max_pkt_sz);
          /* tell receiver our send latency */
          snd_time = (l_int32) send_latency();
          send_ctr_mesg(ctr_buff, snd_time) ;

          /* wait for receiver to start ADR measurement */
          if ((cmd_train_len = recv_ctr_mesg( ctr_buff))  <= 0 )break;
          if ( !quiet )
            printf("ADR train length             :: %ld packets\n",cmd_train_len);
          if ( (cmd_train_len < 10) || (cmd_train_len > TRAIN_LEN) )
            cmd_train_len = TRAIN_LEN;
          if((ret_val=recv_ctr_mesg(ctr_buff)) == -1 )break;
          if ( (((ret_val & CTR_CODE) >> 31) == 1) && ((ret_val & 0x7fffffff) == SEND_TRAIN ) ) 
          {
            if ( !quiet)
              printf("Estimating ADR to initialize rate adjustment algorithm => ");
            fflush(stdout);
            if ( send_train() == -1 )
            {
        close(ctr_strm);
        continue ;
            }
            if ( !quiet)
              printf("Done\n");
          }
          fleet_id=0;
          done=0;
          /* Start avail-bw measurement */
          while(!done)
          {
            if (( ret_val  = recv_ctr_mesg ( ctr_buff ) ) == -1 ) break ;
            if((((ret_val & CTR_CODE) >> 31) == 1) &&((ret_val&0x7fffffff) == TERMINATE)) 
            {
              if ( !quiet)
                printf("Terminating current run.\n");
              done=1;
            }
            else
            {
              transmission_rate = ret_val ;
              if ((cur_pkt_sz = recv_ctr_mesg( ctr_buff)) <= 0 )break;
              if ((stream_len = recv_ctr_mesg( ctr_buff))  <= 0 )break;
              if ((num_stream = recv_ctr_mesg( ctr_buff))  <= 0 )break;
              if ((time_interval = recv_ctr_mesg( ctr_buff)) <= 0 )break;
              if ((ret_val = recv_ctr_mesg ( ctr_buff )) == -1 )break;
              /* ret_val = SENd_FLEET */
              ctr_code = RECV_FLEET | CTR_CODE ;
              if ( send_ctr_mesg(ctr_buff,  ctr_code  ) == -1 ) break;
              if(send_fleet()==-1) break ;
              if ( !quiet) printf("\n");
              fleet_id++ ;
            }
          }
          close(ctr_strm);
        }
      }
    }
  } while (iterate);
  
  return 0;
}

