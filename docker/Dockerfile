from ubuntu:focal as base

ENV LUA_VERSION=5.3
ENV NGINX_VERSION=1.18.0

RUN apt-get update && \
    apt-get install -y \
    tzdata \
    locales

# configure local and timezone
ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8

RUN locale-gen en_US.UTF-8 \
    && echo $LANG > /etc/default/locale

RUN apt-get update && \
    apt-get install -y \
    lua${LUA_VERSION} \
    liblua${LUA_VERSION}-dev \
    nginx=${NGINX_VERSION}-0ubuntu1.4 \
    curl


#####################################################

from base as build

RUN apt-get update && \
    apt-get install -y \
    zlib1g-dev \
    libpcre3-dev \
    make

RUN mkdir nginx-lws

RUN curl -LO https://nginx.org/download/nginx-${NGINX_VERSION}.tar.gz
RUN tar -zxvf nginx-${NGINX_VERSION}.tar.gz

COPY ./ nginx-lws/

WORKDIR nginx-${NGINX_VERSION}
RUN ./configure --with-compat --with-threads --add-dynamic-module=../nginx-lws
RUN make modules

#####################################################

from base as runtime

COPY --from=build \
  ./nginx-${NGINX_VERSION}/objs/lws_module.so \
  /usr/lib/nginx/modules/

COPY examples/lws-examples.conf /etc/nginx/sites-available/default
RUN sed -i '/\slisten\s/d' /etc/nginx/sites-available/default
COPY etc/lws.conf /etc/nginx/modules-available/lws.conf
RUN ln -s /etc/nginx/modules-available/lws.conf /etc/nginx/modules-enabled/lws.conf

COPY examples/ /var/www/lws-examples/
COPY docker/nginx-entrypoint.sh .

EXPOSE 8080

CMD echo "COMMAND IS NOT SET UP"
