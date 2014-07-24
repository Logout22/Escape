#pragma once

#include <sys/common.h>
#include <memory>

//#define IMMP_DEBUG

// from make_control in uielement.h
template<typename T,typename... Args>
inline std::shared_ptr<T> sptr(Args&&... args) {
    return std::shared_ptr<T>(new T(std::forward<Args>(args)...));
}

