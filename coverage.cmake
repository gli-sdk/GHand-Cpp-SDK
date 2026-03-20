execute_process(
    COMMAND gcovr -r ${PROJECT_SOURCE_DIR} --html --html-details -o coverage.html --exclude test/ --exclude googletest/
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)