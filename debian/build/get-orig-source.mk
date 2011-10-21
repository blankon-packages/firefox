#!/usr/bin/make -f

ARGS = -r $(MOZILLA_REPO) -l $(L10N_REPO) -n $(MOZ_APP_NAME) -b $(CURDIR)/debian/config/locales.blacklist

ifdef DEBIAN_TAG
ARGS += -t $(DEBIAN_TAG)
endif

ifdef LOCAL_BRANCH
ARGS += -c $(LOCAL_BRANCH)
endif

get-orig-source:
	python $(CURDIR)/debian/build/create-tarball.py $(ARGS)
