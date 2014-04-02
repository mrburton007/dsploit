#include "hydra-mod.h"
#include "sasl.h"

extern char *HYDRA_EXIT;
char *buf;
static int http_proxy_auth_mechanism = AUTH_ERROR;

int start_http_proxy(int s, char *ip, int port, unsigned char options, char *miscptr, FILE * fp) {
  char *empty = "";
  char *login, *pass, buffer[500], buffer2[500];
  char url[210], host[30];
  char *header = "";            /* XXX TODO */
  char *ptr;

  if (strlen(login = hydra_get_next_login()) == 0)
    login = empty;
  if (strlen(pass = hydra_get_next_password()) == 0)
    pass = empty;

  if (miscptr == NULL) {
    strcpy(url, "http://www.microsoft.com/");
    strcpy(host, "Host: www.microsoft.com\r\n");
  } else {
    sprintf(url, "%.200s", miscptr);
    ptr = strstr(miscptr, "://"); // :// check is in hydra.c
    sprintf(host, "Host: %.200s", ptr + 3);
    if ((ptr = index(host, '/')) != NULL)
      *ptr = 0;
    if ((ptr = index(host + 6, ':')) != NULL && host[0] != '[')
      *ptr = 0;
    strcat(host, "\r\n");
  }

  if (http_proxy_auth_mechanism == AUTH_ERROR) {
    //send dummy request
    sprintf(buffer, "GET %s HTTP/1.0\r\n%sUser-Agent: Mozilla/4.0 (Hydra)\r\n%s\r\n", url, host, header);
    if (hydra_send(s, buffer, strlen(buffer), 0) < 0)
      return 1;

    //receive first 40x
    buf = hydra_receive_line(s);
    while (buf != NULL && strstr(buf, "HTTP/") == NULL) {
      free(buf);
      buf = hydra_receive_line(s);
    }

    if (debug)
      hydra_report(stderr, "S:%s\n", buf);

    //after the first query we should have been disconnected from web server
    s = hydra_disconnect(s);
    if ((options & OPTION_SSL) == 0) {
      s = hydra_connect_tcp(ip, port);
    } else {
      s = hydra_connect_ssl(ip, port);
    }
  }

  if (hydra_strcasestr(buf, "Proxy-Authenticate: Basic") != NULL) {
    http_proxy_auth_mechanism = AUTH_BASIC;
    sprintf(buffer2, "%.50s:%.50s", login, pass);
    hydra_tobase64((unsigned char *) buffer2, strlen(buffer2), sizeof(buffer2));
    sprintf(buffer, "GET %s HTTP/1.0\r\n%sProxy-Authorization: Basic %s\r\nUser-Agent: Mozilla/4.0 (Hydra)\r\n%s\r\n", url, host, buffer2, header);
    if (debug)
      hydra_report(stderr, "C:%s\n", buffer);
    if (hydra_send(s, buffer, strlen(buffer), 0) < 0)
      return 1;
    buf = hydra_receive_line(s);
    while (buf != NULL && strstr(buf, "HTTP/1.") == NULL) {
      free(buf);
      buf = hydra_receive_line(s);
    }

    //if server cut the connection, just exit cleanly or 
    //this will be an infinite loop
    if (buf == NULL) {
      if (verbose)
	hydra_report(stderr, "[ERROR] Server did not answer\n");
      return 3;
    }

    if (debug)
      hydra_report(stderr, "S:%s\n", buf);
  } else {
    if (hydra_strcasestr(buf, "Proxy-Authenticate: NTLM") != NULL) {

      unsigned char buf1[4096];
      unsigned char buf2[4096];
      char *pos = NULL;

      http_proxy_auth_mechanism = AUTH_NTLM;
      //send auth and receive challenge
      //send auth request: let the server send it's own hostname and domainname
      buildAuthRequest((tSmbNtlmAuthRequest *) buf2, 0, NULL, NULL);
      to64frombits(buf1, buf2, SmbLength((tSmbNtlmAuthRequest *) buf2));

      /* to be portable, no snprintf, buffer is big enough so it cant overflow */
      //send the first..
      sprintf(buffer, "GET %s HTTP/1.0\r\n%sProxy-Authorization: NTLM %s\r\nUser-Agent: Mozilla/4.0 (Hydra)\r\nProxy-Connection: keep-alive\r\n%s\r\n", url, host, buf1, header);
      if (hydra_send(s, buffer, strlen(buffer), 0) < 0)
        return 1;

      //receive challenge
      buf = hydra_receive_line(s);
      while (buf != NULL && (pos = hydra_strcasestr(buf, "Proxy-Authenticate: NTLM ")) == NULL) {
        free(buf);
        buf = hydra_receive_line(s);
      }
      if (pos != NULL) {
        char *str;

        pos+=25;
        if ((str=strchr(pos, '\r')) != NULL) {
          pos[str - pos] = 0;
        }
        if ((str=strchr(pos, '\n')) != NULL) {
          pos[str - pos] = 0; }
      }
      //recover challenge
      if (buf != NULL) {
        from64tobits((char *) buf1, pos);
        free(buf);
      }

      //Send response
      buildAuthResponse((tSmbNtlmAuthChallenge *) buf1, (tSmbNtlmAuthResponse *) buf2, 0, login, pass, NULL, NULL);
      to64frombits(buf1, buf2, SmbLength((tSmbNtlmAuthResponse *) buf2));
      sprintf(buffer, "GET %s HTTP/1.0\r\n%sProxy-Authorization: NTLM %s\r\nUser-Agent: Mozilla/4.0 (Hydra)\r\nProxy-Connection: keep-alive\r\n%s\r\n", url, host, buf1, header);
      if (debug)
        hydra_report(stderr, "C:%s\n", buffer);
      if (hydra_send(s, buffer, strlen(buffer), 0) < 0)
        return 1;

      buf = hydra_receive_line(s);
      while (buf != NULL && strstr(buf, "HTTP/1.") == NULL) {
        free(buf);
        buf = hydra_receive_line(s);
      }

      if (buf == NULL)
        return 1;
    } else {
#ifdef LIBOPENSSL
      if (hydra_strcasestr(buf, "Proxy-Authenticate: Digest") != NULL) {

        char *pbuffer;

        http_proxy_auth_mechanism = AUTH_DIGESTMD5;
        pbuffer = hydra_strcasestr(buf, "Proxy-Authenticate: Digest ");
        strncpy(buffer, pbuffer + strlen("Proxy-Authenticate: Digest "), sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        sasl_digest_md5(buffer2, login, pass, buffer, miscptr, "proxy", host, 0, header);
        if (buffer2 == NULL)
          return 3;

        if (debug)
          hydra_report(stderr, "C:%s\n", buffer2);
        if (hydra_send(s, buffer2, strlen(buffer2), 0) < 0)
          return 1;

        buf = hydra_receive_line(s);
        while (buf != NULL && strstr(buf, "HTTP/1.") == NULL) {
          free(buf);
          buf = hydra_receive_line(s);
        }

        if (debug && buf != NULL)
          hydra_report(stderr, "S:%s\n", buf);

        if (buf == NULL)
          return 1;

      } else
#endif
      {
        if (buf != NULL) {
          buf[strlen(buf) - 1] = '\0';
          hydra_report(stderr, "Unsupported Auth type:\n%s\n", buf);
        } else {
          hydra_report(stderr, "Unsupported Auth type\n");
        }
        return 3;
      }
    }
  }

  ptr = ((char *) index(buf, ' ')) + 1;
  if (*ptr == '2' || (*ptr == '3' && *(ptr + 2) == '1') || (*ptr == '3' && *(ptr + 2) == '2')) {
    hydra_report_found_host(port, ip, "http-proxy", fp);
    hydra_completed_pair_found();
  } else {
    if (*ptr != '4')
      hydra_report(stderr, "[INFO] Unusual return code: %c for %s:%s\n", (char) *(index(buf, ' ') + 1), login, pass);
    else
      if (verbose && *(ptr + 2) == '3')
        hydra_report(stderr, "[INFO] Potential success, could be false positive: %s:%s\n", login, pass);
    hydra_completed_pair();
  }

  free(buf);

  if (memcmp(hydra_get_next_pair(), &HYDRA_EXIT, sizeof(HYDRA_EXIT)) == 0)
    return 3;
  return 1;
}

void service_http_proxy(char *ip, int sp, unsigned char options, char *miscptr, FILE * fp, int port) {
  int run = 1, next_run = 1, sock = -1;
  int myport = PORT_HTTP_PROXY, mysslport = PORT_HTTP_PROXY_SSL;

  hydra_register_socket(sp);
  if (memcmp(hydra_get_next_pair(), &HYDRA_EXIT, sizeof(HYDRA_EXIT)) == 0)
    return;

  while (1) {
    next_run = 0;
    switch (run) {
    case 1:                    /* connect and service init function */
      {
        if (sock >= 0)
          sock = hydra_disconnect(sock);
//        usleep(275000);
        if ((options & OPTION_SSL) == 0) {
          if (port != 0)
            myport = port;
          sock = hydra_connect_tcp(ip, myport);
          port = myport;
        } else {
          if (port != 0)
            mysslport = port;
          sock = hydra_connect_ssl(ip, mysslport);
          port = mysslport;
        }

        if (sock < 0) {
          fprintf(stderr, "[ERROR] Child with pid %d terminating, can not connect\n", (int) getpid());
          hydra_child_exit(1);
        }
        next_run = 2;
        break;
      }
    case 2:                    /* run the cracking function */
      next_run = start_http_proxy(sock, ip, port, options, miscptr, fp);
      break;
    case 3:                    /* clean exit */
      if (sock >= 0)
        sock = hydra_disconnect(sock);
      hydra_child_exit(0);
      return;
    default:
      fprintf(stderr, "[ERROR] Caught unknown return code, exiting!\n");
      hydra_child_exit(0);
    }
    run = next_run;
  }
}

int service_http_proxy_init(char *ip, int sp, unsigned char options, char *miscptr, FILE *fp, int port) {
  // called before the childrens are forked off, so this is the function
  // which should be filled if initial connections and service setup has to be
  // performed once only.
  //
  // fill if needed.
  // 
  // return codes:
  //   0 all OK
  //   -1  error, hydra will exit, so print a good error message here

  return 0;
}
