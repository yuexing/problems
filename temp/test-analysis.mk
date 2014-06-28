include new-rules/all-rules.mk

include analysis/ast/emit/unit-test/prog.mk $(CREATE_PROG)
include analysis/test-analysis/unit-test/prog.mk $(CREATE_PROG)
include libs/testanalysis-data/unit-test/prog.mk $(CREATE_PROG)
include libs/testanalysis-util/unit-test/prog.mk $(CREATE_PROG)
include utilities/cov-manage-history/unit-test/prog.mk $(CREATE_PROG)

