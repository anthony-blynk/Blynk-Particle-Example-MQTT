#include "httpc.h"

#define RESPONSE_BUFFER_SIZE 1024

int
strcasecmp (const char *s1, const char *s2)
{
	char	c1, c2;
	int		result = 0;

	while (result == 0)
	{
		c1 = *s1++;
		c2 = *s2++;
		if ((c1 >= 'a') && (c1 <= 'z'))
			c1 = (char)(c1 - ' ');
		if ((c2 >= 'a') && (c2 <= 'z'))
			c2 = (char)(c2 - ' ');
		if ((result = (c1 - c2)) != 0)
			break;
		if ((c1 == 0) || (c2 == 0))
			break;
	}
	return result;
}

int HttpsClient::initTls(const char *rootCaPem, const size_t rootCaPemSize, const char *clientCertPem, const size_t clientCertPemSize,
                const char *clientKeyPem, const size_t clientKeyPemSize) {
    _tlsClient = new Mbedtls;
    return _tlsClient->init(rootCaPem, rootCaPemSize, clientCertPem, clientCertPemSize, clientKeyPem, clientKeyPemSize);
}

int HttpsClient::connect(char *url, uint16_t port)
{
    int ret = _tlsClient->connect(url, port);
    req->url = url;
    return ret;
}

void HttpsClient::disconnect()
{
    _tlsClient->close();
}

HttpsClient::HttpsClient() {
    resp = new HttpResponse;
    resp->body = NULL;
    req = new HttpRequest;
}

HttpsClient::~HttpsClient() {
    _tlsClient->close();
    delete _tlsClient;
    free(resp->body);
    delete resp;
    delete req;
}

int HttpsClient::_sendHeaders(char *path)
{
    switch (req->method) {
        case HTTP_METHOD::POST:
            if (_tlsClient->write((unsigned char *)"POST ", 5) != 5) return -1;
            break;
        case HTTP_METHOD::GET:
            if (_tlsClient->write((unsigned char *)"GET ", 4) != 4) return -1;
            break;
        default:
            return -2;
    }
    if (_tlsClient->write((unsigned char *)path, strlen(path)) != (int)strlen(path) ) return -1;
    if (_tlsClient->write((unsigned char *)" HTTP/1.1\r\nHost: ", 17) != 17 ) return -1;
    if (_tlsClient->write((unsigned char *)req->url, strlen(req->url)) != (int)strlen(req->url) ) return -1;
    if (_tlsClient->write((unsigned char *)"\r\nUser-Agent: particle/1.5.0\r\n", 30) != 30 ) return -1;
    switch (req->content_type) 
    {
        case CONTENT_TYPE::X_WWW_FORM_URLENCODED:
            if (_tlsClient->write((unsigned char *)"Content-Type: application/x-www-form-urlencoded\r\n",49) != 49) return -1;
            break;
        case CONTENT_TYPE::APPLICATION_JSON:
            if (_tlsClient->write((unsigned char *)"Content-Type: application/json\r\n",32) != 32) return -1;
            break;
        default:
            break;
    }
    while (!req->headers.isEmpty()) {
        Header header = req->headers.takeFirst();
        if (_tlsClient->write((unsigned char *)header.name, strlen(header.name)) != (int)strlen(header.name) ) return -1;
        if (_tlsClient->write((unsigned char *)": ", 2) != 2 ) return -1;
        if (_tlsClient->write((unsigned char *)header.value, strlen(header.value)) != (int)strlen(header.value) ) return -1;
        if (_tlsClient->write((unsigned char *)"\r\n", 2) != 2 ) return -1;
    }
    return 0;
}

int HttpsClient::_parseResponse() {
   int ret;
   char buf[RESPONSE_BUFFER_SIZE];
   size_t bufused = 0;
   size_t bodyused = 0;
   size_t bodylen = 0;
   uint8_t state = 0;

   free((void*)resp->body);
   resp->body = NULL;
   resp->headers.clear();
   resp->status_code = 0;

   unsigned long start = millis();

   while((millis() - start) < 10000) {
      ret = _tlsClient->read((unsigned char*)buf + bufused, RESPONSE_BUFFER_SIZE - bufused, 100);

      if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS ||
         ret == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS || ret == MBEDTLS_ERR_SSL_CLIENT_RECONNECT) {
         delay(50);
         continue;
      }
      else if(ret < 0) {
         Log.info("TLS read returned: %d", ret);
         break;
      }
      else if(ret == 0) {
         delay(10);
         continue;
      }

      bufused += ret;
      buf[bufused] = 0; // make safe for strstr etc.

      // --- Parse loop ---
      while(true) {
         if(state < 2) {
            char* p = strstr(buf, "\r\n");
            if(!p)
               break; // wait for more data
            size_t linelen = p - buf;

            if(linelen == 0 && state == 1) {
               // End of headers
               state = (bodylen > 0) ? 3 : 2;
               size_t skip = 2;
               memmove(buf, buf + skip, bufused - skip);
               bufused -= skip;
               continue;
            }

            switch(state) {
            case 0: { // --- Status line ---
               if(strncmp(buf, "HTTP/1.", 7) != 0)
                  return HTTPS_ERROR_BAD_HEADER_FORMAT;

               resp->status_code = (uint16_t)strtol(buf + 9, NULL, 10);
               state = 1;
               size_t skip = linelen + 2;
               memmove(buf, buf + skip, bufused - skip);
               bufused -= skip;
               break;
            }

            case 1: { // --- Header line ---
               Header header;
               if((ret = header.addHeader(buf, linelen)) < 0)
                  return ret;

               resp->headers.append(header);

               if(strcasecmp(header.name, "Content-Length") == 0)
                  bodylen = strtoul(header.value, NULL, 10);

               size_t skip = linelen + 2;
               memmove(buf, buf + skip, bufused - skip);
               bufused -= skip;
               break;
            }
            }
         }
         else if(state == 2) {
            // No body
            return 0;
         }
         else if(state == 3) {
            // Allocate body buffer
            if(bodylen == 0)
               return 0;

            resp->body = (char*)malloc(bodylen + 1);
            if(!resp->body)
               return -1; //HTTPS_ERROR_NO_MEMORY;
            memset(resp->body, 0, bodylen + 1);
            state = 4;
         }
         else if(state == 4) {
            // --- Copy body data ---
            size_t copy = min(bodylen - bodyused, bufused);
            memcpy(resp->body + bodyused, buf, copy);
            bodyused += copy;

            if(copy < bufused) {
               memmove(buf, buf + copy, bufused - copy);
               bufused -= copy;
            }
            else {
               bufused = 0;
            }

            if(bodyused >= bodylen)
               return bodyused; // done
            break;
         }
         else {
            break;
         }
      }
   }

   return -1; // timeout or read error
}

int HttpsClient::postJson(char *path, char *body) {
    int ret;
    req->setMethod(HTTP_METHOD::POST);
    req->setContentType(CONTENT_TYPE::APPLICATION_JSON);
    if ( (ret = _sendHeaders(path)) < 0) return ret;
    char buf[12]; // enough for 32-bit int (up to 4,294,967,295)
    snprintf(buf, sizeof(buf), "%d", strlen(body));
    if (_tlsClient->write((unsigned char *)"Content-Length: ", 16) != 16 ) return -1;
    if (_tlsClient->write((unsigned char *)buf, strlen(buf)) != (int)strlen(buf)) return -1;
    if (_tlsClient->write((unsigned char *)"\r\n\r\n", 4) != 4) return -1;
    if (_tlsClient->write((unsigned char *)body, strlen(body)) != (int)strlen(body)) return -1;
    return _parseResponse();
}

int HttpsClient::postUrlEncoded(char *path) {
    int ret;
    req->setMethod(HTTP_METHOD::POST);
    req->setContentType(CONTENT_TYPE::X_WWW_FORM_URLENCODED);
    if ( (ret = _sendHeaders(path)) < 0) return ret;
    uint16_t body_size=0;
    for(uint8_t i=0;i<req->form_fields.size();i++)
    {
        body_size+=strlen(req->form_fields.at(i).name)+1+strlen(req->form_fields.at(i).value);
        if (i != 0) body_size++;
    }
    char buf[6];
    snprintf(buf, 6, "%d", body_size);
    if (_tlsClient->write((unsigned char *)"Content-Length: ", 16) != 16 ) return -1;
    if (_tlsClient->write((unsigned char *)buf, strlen(buf)) != (int)strlen(buf)) return -1;
    if (_tlsClient->write((unsigned char *)"\r\n\r\n", 4) != 4) return -1;
    while(!req->form_fields.isEmpty())
    {
        NameValue field = req->form_fields.takeFirst();
        if (_tlsClient->write((unsigned char *)field.name, strlen(field.name)) != (int)strlen(field.name) ) return -1;
        if (_tlsClient->write((unsigned char *)"=", 1) != 1 ) return -1;
        if (_tlsClient->write((unsigned char *)field.value, strlen(field.value)) != (int)strlen(field.value) ) return -1;
        if (!req->form_fields.isEmpty()) {
            if (_tlsClient->write((unsigned char *)"&", 1) != 1 ) return -1;
        }
    }
    return _parseResponse();
}

int HttpsClient::get(const char *path) {
    int ret;
    req->setMethod(HTTP_METHOD::GET);
    
    if ((ret = _sendHeaders((char*)path)) < 0) return ret;
    
    if (_tlsClient->write((unsigned char *)"\r\n", 2) != 2) return -1;
    
    return _parseResponse();
}

void HttpsClient::HttpRequest::addFormField(const char *name, const char *value) {
    NameValue field = {name, value};
    form_fields.append(field);
}
void HttpsClient::HttpRequest::addHeader(const char *name, const char *value)
{
    HttpsClient::Header header;
    header.addHeader(name, value);
    headers.append(header);
}

void HttpsClient::HttpRequest::withBasicAuthentication(const char *userid, const char *password)
{
    unsigned char* dst = NULL;
    size_t olen,slen;
    slen = strlen(userid)+strlen(password)+1;
    unsigned char src[slen];
    memcpy(src,userid,strlen(userid));
    memcpy(src+strlen(userid),":",1);
    memcpy(src+strlen(userid)+1,password,strlen(password));
    mbedtls_base64_encode(dst,0,&olen,src,slen);
    dst = new unsigned char[olen+7];
    memset(dst,'\0',olen+7);
    sprintf((char *)dst,"Basic ");
    mbedtls_base64_encode(dst+6,olen,&olen,src,slen);
    addHeader("Authorization",(const char *)dst);
    delete dst;
}

int HttpsClient::Header::addHeader(char *buf, size_t size)
{
    char *k;
    for (k = buf; k < (buf + size); k++)
    {
        if (memcmp(k, ":", 1) == 0)
            break;
    }
    if (k >= (buf + size))
        return HTTPS_ERROR_BAD_HEADER_FORMAT;
    name = new char[k - buf + 1];
    memset((void *)name, '\0', k - buf + 1);
    memcpy((void *)name, (void *)buf, k - buf);
    k++;
    while ((memcmp(k, " ", 1) == 0) && (k < buf + size))
        k++;
    if (k >= (buf + size))
        return HTTPS_ERROR_BAD_HEADER_FORMAT;
    value = new char[buf + size - k + 1];
    memset((void *)value, '\0', buf + size - k + 1);
    memcpy((void *)value, (void *)k, buf + size - k);
    return 0;
}

void HttpsClient::Header::addHeader(const char *name, const char *value)
{
    size_t size = strlen(name) + 1;
    this->name = new char[size];
    memcpy((void *)this->name, (void *)name, size);
    size = strlen(value) + 1;
    this->value = new char[size];
    memcpy((void *)this->value, (void *)value, size);
}