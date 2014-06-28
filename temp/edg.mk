include new-rules/all-rules.mk

SOURCE_DIR:=edg/src

include $(SOURCE_DIR)/mk_errinfo.mk

include $(CREATE_PROG)

include $(SOURCE_DIR)/prog.mk

include $(CREATE_PROG)

include $(SOURCE_DIR)/unit-test/prog.mk

include $(CREATE_PROG)

