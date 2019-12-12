#!/bin/sh -ex

mkdir aquarium
cd aquarium
git init
git remote add origin "https://x-access-token:$GITHUB_PAT@github.com/$GITHUB_REPOSITORY.git"
git fetch origin gh-pages && git reset --hard origin/gh-pages
cd -

cp -r $INSTDIR/. aquarium

cd aquarium
FQDN=`python3 -c 'from socket import *; [ai] = getaddrinfo(gethostname(), None, AF_INET, SOCK_STREAM, IPPROTO_TCP, AI_CANONNAME); print(ai[3])'`
git config user.name "$USER"
git config user.email "$USER@$FQDN"
git add .
git commit -m "Update build"
git push origin master:gh-pages
cd -
