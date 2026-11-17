idf_component_register(SRCS "task_journal.c" "task_nvs.c" "task_core.c"
    INCLUDE_DIRS "." REQUIRES nvs_flash freertos)
