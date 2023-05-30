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
   Author   : Manish Jain ( jain@cc.gatech.edu )
              Constantinos Dovrolis (dovrolis@cc.gatech.edu )
   Release  : Ver 1.3.2
   Support  : This work was supported by the SciDAC
            program of the US department 
--------------------------------------------------*/

/*
 * $Header: /net/cvs/bwtest/pathload/pathload_rcv.c,v 1.139 2006/05/19 22:30:13 jain Exp $
 */

#define LOCAL
#include "pathload_gbls.h"
#include "pathload_rcv.h"


int main(l_int32 argc, char *argv[])
{
  extern char *optarg;
  struct hostent *host_snd;
  // struct sockaddr_in snd_tcp_addr, rcv_udp_addr;
  struct utsname uts ;
  l_int32 ctr_code;
  l_int32 trend, prev_trend = 0;
  l_int32 opt_len, rcv_buff_sz, mss;
  l_int32 ret_val ; 
  l_int32 errflg=0;
  l_int32 file=0;
  char netlogfile[50],filename[50];
  char ctr_buff[8], myname[50], buff[26];
  char mode[4];
  l_int32 c ;
  struct itimerval expireat ;
  struct sigaction sigstruct ;

  slow=0;
  requested_delay = 0;
  interrupt_coalescence=0;
  bad_fleet_cs=0;
  num_stream = NUM_STREAM;
  cmd_num_stream = NUM_STREAM;
  stream_len = STREAM_LEN ;
  cmd_stream_len = STREAM_LEN ;
  cmd_train_len = TRAIN_LEN ;
  exp_flag = 1;
  num=0;
  snd_time_interval=0;
  cmd_max_fleets = 0;
  overhead = 0;
  cmd_max_overhead = 0;

  converged_gmx_rmx = 0 ;
  converged_gmn_rmn = 0 ;
  converged_rmn_rmx = 0 ;
  counter = 0 ;
  prev_actual_rate = 0;
  prev_req_rate = 0 ; 
  cur_actual_rate = 0 ;
  cur_req_rate = 0 ;
  gettimeofday(&exp_start_time, NULL);
  verbose=1;
  bw_resol=0;
  netlog=0;
  increase_stream_len=0;
  lower_bound=0;

  /* Additional var for support IPv6 */
  struct sockaddr_storage snd_tcp_addr, rcv_udp_addr;
  struct addrinfo hints, *res, *res0;
  ssize_t l;
  int s;
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  int error;
  int on;
  socklen_t rcv_udp_addr_len = sizeof(rcv_udp_addr);
  socklen_t snd_tcp_addr_len = sizeof(snd_tcp_addr);
  int v6mode = 0;

  if ( argc == 1 ) errflg++ ;
  while ((c = getopt(argc, argv, "6t:s:hw:k:n:f:b:vHqo:O:N:V")) != EOF)
    switch (c) 
    {
      case '6':
        v6mode = 1;
        printf("Enable IPv6 mode\n");
        break ;
      case 't':
        requested_delay = atoi(optarg);
        break;
      case 's':
        strcpy(hostname,optarg);
        break;
      case 'w':
        bw_resol = atof(optarg);
        break;
      case 'k':
        cmd_stream_len = atoi(optarg);
	cmd_train_len = cmd_stream_len * TRAIN_LEN / STREAM_LEN ;
        break;
      case 'n':
        cmd_num_stream = atoi(optarg);
	num_stream = cmd_num_stream;
        break;
      case 'f':
        cmd_max_fleets = atoi(optarg);
        break;
      case 'b':
        cmd_max_overhead = atoi(optarg);
        break;
      case 'q':
        Verbose=0;
        verbose=0;
        break;
      case 'v':
        Verbose=1;
        break;
      case 'O':
        file=1;
        strcpy(filename,optarg);
        strcpy(mode,"a");
        break;
      case 'o':
        file=1;
        strcpy(filename,optarg);
        strcpy(mode,"w");
        break;
      case 'H':
      case 'h':
        help() ;
        break ;
      case 'N':
        netlog=1; 
        strcpy(netlogfile,optarg);
        break;
      case 'V':
        VVerbose = 1;
        break ;
      case '?':
        errflg++;
        break ;
    }
  if (errflg)
  {
    fprintf(stderr, "usage: pathload_rcv [-q|-v] [-o <filename>] [-N <filename>]\
[-w <bw_resol>] [-h|-H] -s <sender>\n");
    exit (0);
  }

  if (netlog)
    netlog_fp = fopen(netlogfile,"a");
  if (file)
    pathload_fp = fopen(filename,mode);
  else
    pathload_fp = fopen("pathload.log" , "a" ) ;
  fprintf(pathload_fp, "\n\n");

  strncpy(buff, ctime(&(exp_start_time.tv_sec)), 24);
  buff[24] = '\0';
  bzero(myname,50);
  if ( gethostname(myname ,50 ) == -1 )
  {
    if ( uname(&uts) < 0 )
      strcpy(myname , "UNKNOWN") ;
    else
      strcpy(myname , uts.nodename) ;
  }
  if (verbose || Verbose)
    printf("\n\nReceiver %s starts measurements at sender %s on %s \n", myname , hostname, buff);
  fprintf(pathload_fp,"\n\nReceiver %s starts measurements at sender %s on %s \n", myname , hostname, buff);

  // IPv6 support
  /* Create UDP socket to recieve probe packets */
  snprintf(sbuf, sizeof(sbuf), "%u", UDPRCV_PORT);
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;
  // TODO: Do not hard-coding 
  if (v6mode) hints.ai_family = AF_INET6;
  else hints.ai_family = AF_INET;

  error = getaddrinfo(NULL, sbuf, &hints, &res0);
  if (error)
  {
    fprintf(stderr, "%s: %s\n", sbuf, gai_strerror(error));
    exit(EXIT_FAILURE);
  }
  error = getnameinfo(res0->ai_addr, res0->ai_addrlen,
                      hbuf, sizeof(hbuf), sbuf, sizeof(sbuf),
                      NI_NUMERICHOST | NI_NUMERICSERV);
  if (error)
  {
    fprintf(stderr, "%s %s: %s\n", hbuf, sbuf, gai_strerror(error));
    exit(-1);
  }
  fprintf(stderr, "Listen to %s port %s\n", hbuf, sbuf);

  sock_udp = socket(res0->ai_family, res0->ai_socktype, res0->ai_protocol);
  if (sock_udp < 0)
  {
    exit(-1);
  }

  if (bind(sock_udp, res0->ai_addr, res0->ai_addrlen) < 0)
  {
    perror("ERROR :: failed to bind DGRAM socket:");
    exit(-1);
  }

  freeaddrinfo(res0);
  getsockname(sock_udp, (struct sockaddr *)&rcv_udp_addr, &rcv_udp_addr_len);

  
  rcv_buff_sz = UDP_BUFFER_SZ;
  if (setsockopt(sock_udp, SOL_SOCKET, SO_RCVBUF, &rcv_buff_sz, sizeof(rcv_buff_sz)) < 0)
  {
    rcv_buff_sz/=2;
    if (setsockopt(sock_udp, SOL_SOCKET, SO_RCVBUF, &rcv_buff_sz, sizeof(rcv_buff_sz)) < 0)
    {
      printf("ERROR :: Unable to set socket buffer to %d .\n",rcv_buff_sz);
      exit(-1);
    }
  }
  opt_len = 1;
  if (setsockopt(sock_udp, SOL_SOCKET, SO_REUSEADDR, &opt_len, sizeof(opt_len)) < 0)
  {
    perror("setsockopt(sock_udp,SOL_SOCKET,SO_REUSEADDR):");
    exit(-1);
  }
  



  /* Create TCP socket to control stream */
  snprintf(sbuf, sizeof(sbuf), "%u", TCPSND_PORT);
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  error = getaddrinfo(hostname, sbuf, &hints, &res0);
  if (error)
  {
    fprintf(stderr, "%s: %s\n", sbuf, gai_strerror(error));
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
    fprintf(stderr, "Trying %s port %s ...\n", hbuf, sbuf);
    if (rcv_udp_addr.ss_family == AF_INET && res->ai_family == AF_INET6) {
      fprintf(stderr, "Address family missmatch\n");
      continue;
    }

    sock_tcp = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock_tcp < 0)
    {
      continue;
    }
    if (res->ai_family == AF_INET6 &&
        setsockopt(sock_tcp, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0)
    {
      perror("setsockopt");
      continue;
    }
    if (connect(sock_tcp, res->ai_addr, res->ai_addrlen) < 0)
    {
      close(sock_tcp);
      sock_tcp = -1;
      continue;
    }

    break;
  }
  if (res == NULL)
  {
    fprintf(stderr, "Unable to connect to %s port %s\n", hbuf, sbuf);
    exit(EXIT_FAILURE);
  }
  freeaddrinfo(res0);
  getsockname(sock_tcp, (struct sockaddr *)&snd_tcp_addr, &snd_tcp_addr_len);

  opt_len = 1;
  if (setsockopt(sock_tcp, SOL_SOCKET, SO_REUSEADDR, &opt_len, sizeof(opt_len)) < 0)
  {
    perror("setsockopt(sock_tcp,SOL_SOCKET,SO_REUSEADDR):");
    exit(-1);
  }

  if (fcntl(sock_tcp, F_SETFL, O_NONBLOCK) < 0)
  {
    perror("fcntl:");
    exit(-1);
  }


  /*
    measure max_pkt_sz (based on TCP MSS).
    this is not accurate because it does not take into
    account MTU of intermediate routers.
  */
  opt_len = sizeof(mss);
  if (getsockopt(sock_tcp, IPPROTO_TCP, TCP_MAXSEG, (char*)&mss,(socklen_t *) &opt_len)<0)
  {
     perror("getsockopt(sock_tcp,IPPROTO_TCP,TCP_MAXSEG):");
     exit(-1);
  }
  rcv_max_pkt_sz = mss;
  if (rcv_max_pkt_sz == 0 || rcv_max_pkt_sz == 1448 ) 
    rcv_max_pkt_sz = 1472;   /* Make it Ethernet sized MTU */
  else
    rcv_max_pkt_sz = mss+12;

  sigstruct.sa_handler = sig_alrm ;
  sigemptyset(&sigstruct.sa_mask);
  sigstruct.sa_flags = 0 ;
  #ifdef SA_INTERRUPT
    sigstruct.sa_flags |= SA_INTERRUPT ;
  #endif
  sigaction(SIGALRM , &sigstruct,NULL );
  expireat.it_value.tv_sec = 60 ; /* RECEIVER ABORTS TIME */
  expireat.it_value.tv_usec = 0 ;
  expireat.it_interval.tv_sec = 0 ; 
  expireat.it_interval.tv_usec = 0 ;
  setitimer(ITIMER_REAL, &expireat,NULL);

  /* receive sender max_pkt_sz */
  while ((snd_max_pkt_sz = recv_ctr_mesg(sock_tcp, ctr_buff)) == -1);
  if ( snd_max_pkt_sz == -2 )
  {
    printf("pathload_snd did not respond for 60 sec\n");
    close(sock_tcp);
    exit(-1);
  }
  expireat.it_value.tv_sec = 0 ; 
  expireat.it_value.tv_usec = 0 ;
  expireat.it_interval.tv_sec = 0 ; 
  expireat.it_interval.tv_usec = 0 ;
  setitimer(ITIMER_REAL, &expireat,NULL);
  
  sigstruct.sa_handler = SIG_DFL ;
  sigemptyset(&sigstruct.sa_mask);
  sigstruct.sa_flags = 0 ;
  sigaction(SIGALRM , &sigstruct,NULL );


  send_ctr_mesg(ctr_buff, rcv_max_pkt_sz);
  max_pkt_sz = (rcv_max_pkt_sz < snd_max_pkt_sz) ? rcv_max_pkt_sz:snd_max_pkt_sz ;
  if (Verbose)
    printf("  Maximum packet size          :: %ld bytes\n",max_pkt_sz);
  fprintf(pathload_fp,"  Maximum packet size          :: %ld bytes\n",max_pkt_sz);
  rcv_latency = (l_int32) recvfrom_latency(rcv_udp_addr, rcv_udp_addr_len);
  while ((snd_latency = recv_ctr_mesg(sock_tcp, ctr_buff)) == -1);
  if (Verbose)
  {
    printf("  send latency @sndr           :: %ld usec\n",snd_latency);
    printf("  recv latency @rcvr           :: %ld usec\n",rcv_latency);
  }
  fprintf(pathload_fp,"  send latency @sndr           :: %ld usec\n",snd_latency);
  fprintf(pathload_fp,"  recv latency @rcvr           :: %ld usec\n",rcv_latency);
  min_time_interval=
      SCALE_FACTOR*((rcv_latency>snd_latency)?rcv_latency:snd_latency) ;
  min_time_interval = min_time_interval>MIN_TIME_INTERVAL?
      min_time_interval:MIN_TIME_INTERVAL;
  if (Verbose)
    printf("  Minimum packet spacing       :: %ld usec\n",min_time_interval );
  fprintf(pathload_fp,"  Minimum packet spacing       :: %ld usec\n",min_time_interval );
  max_rate = (max_pkt_sz+28) * 8. / min_time_interval ;
  min_rate = (MIN_PKT_SZ+28) * 8./ MAX_TIME_INTERVAL ;
  if(Verbose)
    printf("  Max rate(max_pktsz/min_time) :: %.2fMbps\n",max_rate);
  fprintf(pathload_fp,"  Max rate(max_pktsz/min_time) :: %.2fMbps\n",max_rate);

  /* Estimate ADR */
  adr = get_adr() ;
  if ( bw_resol == 0 && adr != 0 )
    bw_resol = .02*adr ;
  else if (bw_resol == 0 )
    bw_resol = 2 ; 
  if(Verbose)
    printf("  Grey bandwidth resolution    :: %.2f\n",grey_bw_resolution());
  fprintf(pathload_fp,"  Grey bandwidth resolution    :: %.2f\n",grey_bw_resolution());
  
  if (interrupt_coalescence)
  {
    bw_resol = .05*adr;
    if(verbose||Verbose)
      printf("  Interrupt coalescion detected\n");
    fprintf(pathload_fp,"  Interrupt coalescion detected\n");
  }
  
  if ( adr == 0 || adr > max_rate || adr < min_rate)
    tr = (max_rate+min_rate)/2.;
  else 
    tr = adr ;

  /* Estimate the available bandwidth.*/
  transmission_rate = (l_uint32)rint(1000000 * tr);
  max_rate_flag = 0 ;
  min_rate_flag = 0 ; 
  fflush(pathload_fp);

  sigemptyset(&sigstruct.sa_mask);
  sigstruct.sa_handler = sig_alrm ;
  sigstruct.sa_flags = 0 ;
  #ifdef SA_INTERRUPT
    sigstruct.sa_flags |= SA_INTERRUPT ;
  #endif
  sigaction(SIGALRM , &sigstruct,NULL );

  if ( requested_delay )
  {
    expireat.it_value.tv_sec = requested_delay ; /* RECEIVER ABORTS TIME */
    expireat.it_value.tv_usec = 0 ;
    expireat.it_interval.tv_sec = 0 ; 
    expireat.it_interval.tv_usec = 0 ;
    setitimer(ITIMER_REAL, &expireat,NULL);
  }


  while (1)
  {
    if ( calc_param() == -1 )
    {
      ctr_code = TERMINATE | CTR_CODE;
      send_ctr_mesg(ctr_buff, ctr_code);
      terminate_gracefully(exp_start_time) ;
    }
    send_ctr_mesg(ctr_buff, transmission_rate);
    send_ctr_mesg(ctr_buff,cur_pkt_sz) ;
    if ( increase_stream_len )
      stream_len=3*cmd_stream_len;
    else
      stream_len = cmd_stream_len;
    send_ctr_mesg(ctr_buff,stream_len);
    send_ctr_mesg(ctr_buff,num_stream);
    send_ctr_mesg(ctr_buff,time_interval);
    ctr_code = SEND_FLEET | CTR_CODE ;
    send_ctr_mesg(ctr_buff, ctr_code);
    overhead += cur_pkt_sz * stream_len * num_stream;

    while (1)
    {
      ret_val = recv_ctr_mesg(sock_tcp, ctr_buff);
      if ((((ret_val & CTR_CODE) >> 31) == 1) &&    
           ((ret_val & 0x7fffffff) == RECV_FLEET )) 
        break ;
      else if ( (((ret_val & CTR_CODE) >> 31) == 1) &&    
                 ((ret_val & 0x7fffffff) == FINISHED_STREAM )) 
        ret_val = recv_ctr_mesg(sock_tcp, ctr_buff);
    }

    if (recv_fleet() == -1)
    {
      if ( !increase_stream_len )
      {
        trend = INCREASING;
        if ( exp_flag == 1 && prev_trend != 0 && prev_trend != trend)
          exp_flag = 0;
        prev_trend = trend;
        if (rate_adjustment(INCREASING) == -1)
          terminate_gracefully(exp_start_time);
      }
      if ( (cmd_max_fleets != 0) && (exp_fleet_id > cmd_max_fleets) )
        terminate_gracefully(exp_start_time) ;	
      if ( (cmd_max_overhead != 0) && (overhead > cmd_max_overhead) )
        terminate_gracefully(exp_start_time) ;	
    }
    else
    {
      get_sending_rate() ;
      trend = aggregate_trend_result();

      if ( (cmd_max_fleets != 0) && (exp_fleet_id > cmd_max_fleets) )
        terminate_gracefully(exp_start_time) ;	
      if ( (cmd_max_overhead != 0) && (overhead > cmd_max_overhead) )
        terminate_gracefully(exp_start_time) ;	
      if ( trend == -1 && bad_fleet_cs && retry_fleet_cnt_cs >NUM_RETRY_CS )
        terminate_gracefully(exp_start_time) ;
      else if(( trend == -1 && bad_fleet_cs && retry_fleet_cnt_cs <= NUM_RETRY_CS )) /* repeat fleet with current rate. */
        continue ;

      if (trend != GREY)
      {
        if (exp_flag == 1 && prev_trend != 0 && prev_trend != trend)
          exp_flag = 0;
        prev_trend = trend;
      }

      if (rate_adjustment(trend) == -1)
        terminate_gracefully(exp_start_time);
    }
    fflush(pathload_fp);
  }
}
