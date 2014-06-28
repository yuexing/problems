# relay.mk
# Just include another Makefile as the only one, but similar to
# as if it was included by the full build.

include new-rules/all-rules.mk

include $(TARGETMK)

# EOF
