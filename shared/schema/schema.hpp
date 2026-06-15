#pragma once

#define CREATION_WRAPPER(creation_function) []<class ...Args>(Args&&... args) { return creation_function(std::forward<Args>(args)...); }
