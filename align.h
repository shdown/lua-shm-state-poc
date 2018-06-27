#ifndef align_h_
#define align_h_

#ifndef DUMB_ALIGN
#   error "Please define DUMB_ALIGN."
#endif

// Assumes /S_/ is a power of 2. If that's not the case, use the following (slower) version:
//     #define ALIGN_TO(N_, S_) (((((N_) - 1) / (S_)) + 1) * (S_))
#define ALIGN_TO(N_, S_) (((N_) + (S_) - 1) & ~((S_) - 1))

#define ALIGN_TO_DUMB(N_) ALIGN_TO(N_, DUMB_ALIGN)

#endif
