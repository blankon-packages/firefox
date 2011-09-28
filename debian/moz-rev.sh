#!/bin/sh

# Copyright (c) 2009 Fabien Tassin <fta@sofaraway.org>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

URL=$1
BRANCH=$2

set -e

if [ Z$1 = Z ] ; then
  echo "Usage: $0 MOZ_URL [BRANCH]"
  echo
  echo "Ex: $0 http://hg.mozilla.org/releases/mozilla-1.9.2 default"
  exit 1
fi

if [ Z$BRANCH = Z ] ; then
  BRANCH="default"
fi

# Get the tip of $BRANCH
URL_BASE=`echo $URL | sed -e 's,^\(http://[^/]*\)/.*,\1,'`
TIP=`wget -qO- $URL/summary | sed -e '1,/>branches</d' | grep ">$BRANCH<" | \
 sed -e 's,.*<td>'$BRANCH'</td><td class="link"><a href="\([^"]*\)">changeset</a>.*,\1,'`

# Get the rev-id and hash of this top
REVS=`wget -qO- $URL_BASE$TIP | grep '^<title>' | sed -e 's/.* changeset \([^<]*\).*/\1/'`
REV=`echo $REVS | cut -d: -f1`
HASH=`echo $REVS | cut -d: -f2`

# Get the date of this tip
DATE=`wget -qO- "$URL/pushlog?changeset=$HASH" | grep '^ <updated>' | tr '<>' '"' | cut -d'"' -f3 | cut -dT -f1 | tr -d '-'`

echo "${DATE}r${REV}"
