PROTO_LIBRARY()

OWNER(g:solomon)

SRCS(
    metric_meta.proto
)

IF (NOT PY_PROTOS_FOR)
    EXCLUDE_TAGS(GO_PROTO)
ENDIF()

END()
