#include <curl/curl.h>
#include <curl/easy.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void assert_curl(CURLcode rv, const char* op)
{
  if( rv != CURLE_OK ) {
    fprintf(stderr, "%s failed with %d, %s\n", 
        op, rv, curl_easy_strerror(rv));

//    abort();
  }
}

void assert_mcurl(CURLMcode rv, const char* op)
{
  if( rv != CURLE_OK ) {
    fprintf(stderr, "%s failed with %d, %s\n", 
        op, rv, curl_multi_strerror(rv));

    abort();
  }
}

void setup()
{
  assert_curl( curl_global_init(CURL_GLOBAL_ALL), "curl_global_init" );
}

void teardown()
{
   curl_global_cleanup();
}

// silently drop
size_t drop_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  return size * nmemb;
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  for(int i = 0; i < size * nmemb; ++i) {
    printf("%c", ptr[i]);
  }
  return size * nmemb;
}

size_t header_callback(char *buffer,   size_t size,   size_t nitems,   void *userdata)
{
  for(int i = 0; i < size * nitems; ++i) {
    printf("%c", buffer[i]);
  }
  return size * nitems;
}

void test_download_http()
{
  CURL *ch = curl_easy_init();

  if(!ch) {
    return;
  }

  assert_curl( curl_easy_setopt(ch, CURLOPT_URL, "http://www.google.com/"), 
      "curl_easy_setopt(url)" );

  assert_curl( curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1L), 
      "curl_easy_setopt(redirect)" );

  assert_curl( curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, write_callback), 
      "curl_easy_setopt(write)" );

  assert_curl( curl_easy_setopt(ch, CURLOPT_HEADERFUNCTION, header_callback), 
      "curl_easy_setopt(header)" );

  assert_curl( curl_easy_perform(ch), "curl_easy_perform" );
  
  curl_easy_cleanup(ch);
}

static const char index_html[] = "<html><head><title>Index</title></head><body>A Test Page</body></html>";
static int offset = 0;

size_t http_put_read(void *ptr, size_t size, size_t nmemb, void *stream)
{
  printf("http_put_read: %lu, %d, %lu, %lu\n", sizeof(index_html), offset, size, nmemb);

  memcpy(ptr, index_html + offset, size * nmemb);
  offset += size * nmemb;

  return size * nmemb;
}

void test_http_put()
{
  offset = 0;
  CURL *ch = curl_easy_init();

  if(!ch) {
    return;
  }

  assert_curl( curl_easy_setopt(ch, CURLOPT_READFUNCTION, http_put_read), "read");
  assert_curl( curl_easy_setopt(ch, CURLOPT_UPLOAD, 1L), "upload");
  assert_curl( curl_easy_setopt(ch, CURLOPT_PUT, 1L), "put");
  assert_curl( curl_easy_setopt(ch, CURLOPT_URL, "http://tiantianxu.net/index.html"), "url");
  assert_curl( curl_easy_setopt(ch, CURLOPT_INFILESIZE_LARGE, (curl_off_t)(sizeof(index_html)+1)), "size");
  
  assert_curl( curl_easy_perform(ch), "curl_easy_perform" );
  curl_easy_cleanup(ch);
}

CURL* get_easy()
{
  CURL *ch = curl_easy_init();

  if(!ch) {
    return ch;
  }

  assert_curl( curl_easy_setopt(ch, CURLOPT_URL, "http://176.32.98.166"), 
      "curl_easy_setopt(url)" );

  assert_curl( curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1L), 
      "curl_easy_setopt(redirect)" );

  assert_curl( curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, drop_write_callback), 
      "curl_easy_setopt(write)" );


  return ch;
}

void test_multi(int num)
{
  CURLM* multi = curl_multi_init();

  CURL** arr = (CURL**)malloc(sizeof(CURL*) * num);

  for(int i = 0; i < num; ++i) {
    arr[i] = get_easy();
    assert_mcurl( curl_multi_add_handle(multi, arr[i]), "add_mult");
  }

  printf("Done creating\n");

// third version:
// keep them, each 128 do a perform.
  int still_running = 0;
  do {
    printf("\n\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
    struct timeval timeout = {1, 0};
    
    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexcep;
    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);

    long curl_timeo = -1;
    assert_mcurl (curl_multi_timeout(multi, &curl_timeo), "timeout");
    if(curl_timeo > 0) {
      timeout.tv_sec = curl_timeo / 1000;
      if(timeout.tv_sec > 1) {
        timeout.tv_sec = 1;
      } else {
        timeout.tv_usec = (curl_timeo % 1000) * 1000;
      }
    } // negative means no need to wait, then use default 1000 usec

    printf("LOOP curl_multi_timeout says: %ld\n", curl_timeo);

    int maxfd = -1;
    assert_mcurl( curl_multi_fdset(multi, &fdread, &fdwrite, &fdexcep, &maxfd), "fdset" );

    printf("LOOP curl_multi_fdset says: %d\n", maxfd);

    // when maxfd == 0, this is select(0,...) equivalent to sleep timeout
    int rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);

    switch(rc) {
      case -1:
        /* select error */ 
        break;
      case 0: /* timeout */ 
      default: /* action */ 
        curl_multi_perform(multi, &still_running);
        break;
    }

    /* See how the transfers went */ 
    CURLMsg* msg = NULL;
    int msgs_left = 0;
    while ((msg = curl_multi_info_read(multi, &msgs_left))) {
      if (msg->msg == CURLMSG_DONE) {
        int idx;

        /* Find out which handle this message is about */ 
        for (idx=0; idx<num; idx++) {
          if (msg->easy_handle == arr[idx])
            break;
        }

        if(idx != num) {
          printf("The %d curl completed with %d, %s\n", 
              idx, msg->data.result, curl_easy_strerror(msg->data.result));
        }
      }
    }

  } while(still_running);

  curl_multi_cleanup(multi);

  for(int i = 0; i < num; ++i) {
    curl_easy_cleanup(arr[i]);
  }
  free(arr);
}

int main(int argc, const char** argv) 
{
  setup();
  
//  test_download_http();
//   test_http_put();
  int num = 100;
  if(argc > 0) {
    int proposed = atoi(argv[1]);
    if(proposed > 0) {
      num = proposed ;
    }
  }
  test_multi(num);

  teardown();
  return 0;
}
