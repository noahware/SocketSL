#pragma once

#define CREATION_WRAPPER(creation_function) []<class ...arguments_t>(arguments_t&&... arguments) { return creation_function(std::forward<arguments_t>(arguments)...); }
