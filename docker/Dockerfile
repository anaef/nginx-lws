FROM ubuntu:26.04 AS base

ENV LUA_VERSION=5.4
ENV NGINX_VERSION=1.28.3
ENV NGINX_PACKAGE_VERSION=1.28.3-2ubuntu1.8

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
    nginx=${NGINX_PACKAGE_VERSION}


#####################################################

FROM base AS build

RUN sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources && \
    apt-get update && \
    apt-get install -y \
    build-essential \
    zlib1g-dev \
    libpcre2-dev

RUN mkdir nginx-lws

RUN source_directory="$(mktemp -d)" && \
    cd "${source_directory}" && \
    apt-get source "nginx=${NGINX_PACKAGE_VERSION}" && \
    mv "nginx-${NGINX_VERSION}" / && \
    cd / && \
    rm -rf "${source_directory}"

COPY ./ nginx-lws/

WORKDIR nginx-${NGINX_VERSION}
RUN ./configure --with-compat --with-threads --add-dynamic-module=../nginx-lws
RUN make modules

#####################################################

FROM base AS runtime

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
