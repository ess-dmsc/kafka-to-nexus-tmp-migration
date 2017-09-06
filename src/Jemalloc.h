#pragma once

#include <jemalloc/jemalloc.h>
#include <map>
#include <memory>
#include <mutex>

static void jemcb(void *cbd, char const *s) { fwrite(s, 1, strlen(s), stdout); }

class Jemalloc;

static std::map<void *, Jemalloc *> g_jems;
static std::mutex g_mx;

static void *extent_alloc(extent_hooks_t *extent_hooks, void *addr, size_t size,
                          size_t align, bool *zero, bool *commit,
                          unsigned arena);

static char const *errname(int err) {
  switch (err) {
  case EINVAL:
    return "EINVAL";
  case ENOENT:
    return "ENOENT";
  case EPERM:
    return "EPERM";
  case EAGAIN:
    return "EAGAIN";
  case EFAULT:
    return "EFAULT";
  }
  return "UNKNOWN";
}

class Jemalloc {
public:
  using ptr = std::unique_ptr<Jemalloc>;
  using sptr = std::shared_ptr<Jemalloc>;

  static sptr create(void *base, void *ceil) {
    auto ret = sptr(new Jemalloc);
    ret->base = base;
    ret->alloc_base = ret->base;
    ret->alloc_ceil = ceil;
    char const *jemalloc_version = nullptr;
    ;
    size_t n = 0;
    n = sizeof(char const *);
    mallctl("version", &jemalloc_version, &n, nullptr, 0);
    LOG(3, "jemalloc version: {}", jemalloc_version);
    unsigned narenas = 0;
    n = sizeof(narenas);
    mallctl("arenas.narenas", &narenas, &n, nullptr, 0);
    LOG(3, "arenas.narenas: {}", narenas);

    std::memset(&ret->hooks, 0, sizeof(extent_hooks_t));
    auto self = ret.get();
    {
      std::unique_lock<std::mutex> lock(g_mx);
      g_jems[&ret->hooks] = self;
    }
    ret->hooks.alloc = extent_alloc;
    extent_hooks_t *hooks_ptr = &ret->hooks;
    n = sizeof(unsigned);
    int err = mallctl("arenas.create", &ret->aix, &n, &hooks_ptr,
                      sizeof(extent_hooks_t *));
    // int err = mallctl("arenas.create", &aix, &n, nullptr, 0);
    if (err != 0) {
      LOG(3, "error in mallctl arenas.create: {}", errname(err));
      exit(1);
    }
    LOG(3, "arena created: {}", ret->aix);

    // void * big = mallocx(80 * 1024 * 1024, );

    return ret;
  }

  void stats() const { malloc_stats_print(jemcb, nullptr, ""); }

  void *alloc(size_t size) {
    auto addr = mallocx(size, MALLOCX_ARENA(aix));
    if (addr == nullptr) {
      LOG(3, "fail alloc size: {}", size);
      exit(1);
    }
    return addr;
  }

  void *alloc_base = nullptr;
  void *alloc_ceil = nullptr;

private:
  Jemalloc() {}
  unsigned aix = -1;
  void *base = nullptr;
  extent_hooks_t hooks;
  std::function<void *(extent_hooks_t *extent_hooks, void *addr, size_t size,
                       size_t align, bool *zero, bool *commit, unsigned arena)>
      f_alloc;
};

static void *extent_alloc(extent_hooks_t *extent_hooks, void *addr, size_t size,
                          size_t align, bool *zero, bool *commit,
                          unsigned arena) {
  LOG(3, "extent_alloc arena: {}  size: {}  align: {}  zero: {}  commit: {}",
      arena, size, align, *zero, *commit);
  if (addr != nullptr) {
    LOG(3, "error addr is set");
    exit(1);
  }
  std::unique_lock<std::mutex> lock(g_mx);
  auto jm = g_jems[extent_hooks];
  auto base = (char const *)jm->alloc_base;
  auto ceil = (char const *)jm->alloc_ceil;
  if (base + size > ceil) {
    return nullptr;
  }
  auto q = (void *)base;
  base += size;
  jm->alloc_base = (void *)base;
  if (*zero) {
    std::memset(q, 0, size);
  }
  return q;
}
