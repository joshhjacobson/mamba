// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_ENVIRONMENT_HPP
#define MAMBA_CORE_ENVIRONMENT_HPP

#include <map>
#include <cstdlib>
#include <cassert>
#include <string_view>
#include <optional>
#include "mamba/core/output.hpp"

#include "mamba_fs.hpp"
#include "util.hpp"

#ifdef _WIN32
#include <Shlobj.h>;
#endif

#ifndef _WIN32
#include <sys/utsname.h>
#include <wordexp.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

extern "C"
{
    extern char** environ;
}
#endif

namespace mamba
{
    namespace env
    {
        constexpr inline const char* pathsep()
        {
#ifdef _WIN32
            return ";";
#else
            return ":";
#endif
        }

        inline std::optional<std::string> get(const std::string& key)
        {
#ifdef _WIN32
            const size_t initial_size = 1024;
            std::unique_ptr<char[]> temp_small = std::make_unique<char[]>(initial_size);
            std::size_t size = GetEnvironmentVariableA(key.c_str(), temp_small.get(), initial_size);
            if (size == 0)  // Error or empty/missing
            {
                // Note that on Windows environment variables can never be empty,
                // only missing. See https://stackoverflow.com/a/39095782
                auto last_err = GetLastError();
                if (last_err != ERROR_ENVVAR_NOT_FOUND)
                {
                    LOG_ERROR << "Could not get environment variable: " << last_err;
                }
                return {};
            }
            else if (size > initial_size)  // Buffer too small
            {
                std::unique_ptr<char[]> temp_large = std::make_unique<char[]>(size);
                GetEnvironmentVariableA(key.c_str(), temp_large.get(), size);
                std::string res(temp_large.get());
                return res;
            }
            else  // Success
            {
                std::string res(temp_small.get());
                return res;
            }
#else
            const char* value = std::getenv(key.c_str());
            if (value)
                return value;
            else
                return {};
#endif
        }

        inline bool set(const std::string& key, const std::string& value)
        {
#ifdef _WIN32
            auto res = SetEnvironmentVariableA(key.c_str(), value.c_str());
            if (!res)
            {
                LOG_ERROR << "Could not set environment variable: " << GetLastError();
            }
            return res;
#else
            return setenv(key.c_str(), value.c_str(), 1) == 0;
#endif
        }

        inline void unset(const std::string& key)
        {
#ifdef _WIN32
            auto res = SetEnvironmentVariableA(key.c_str(), NULL);
            if (!res)
            {
                LOG_ERROR << "Could not unset environment variable: " << GetLastError();
            }
#else
            unsetenv(key.c_str());
#endif
        }

        inline fs::path which(const std::string& exe)
        {
            // TODO maybe add a cache?
            auto env_path = env::get("PATH");
            if (env_path)
            {
                std::string path = env_path.value();
                auto parts = mamba::split(path, pathsep());
                for (auto& p : parts)
                {
                    if (!fs::exists(p) || !fs::is_directory(p))
                    {
                        continue;
                    }
                    for (const auto& entry : fs::directory_iterator(p))
                    {
                        if (entry.path().filename() == exe)
                        {
                            return entry.path();
                        }
                    }
                }
            }
            return "";  // empty path
        }

        inline std::map<std::string, std::string> copy()
        {
            std::map<std::string, std::string> m;
#ifndef _WIN32
            int i = 1;
            const char* c = *environ;
            for (; c; i++)
            {
                std::string_view s(c);
                auto pos = s.find("=");
                m[std::string(s.substr(0, pos))]
                    = (pos != s.npos) ? std::string(s.substr(pos + 1)) : "";
                c = *(environ + i);
            }
#else

            // inspired by
            // https://github.com/gennaroprota/breath/blob/0709a9f0fe4e745b1d9fc44ab65d92853820b515
            //                    /breath/environment/brt/dep/syst/windows/get_environment_map.cpp#L38-L80
            char* start = GetEnvironmentStrings();
            if (start == nullptr)
            {
                throw std::runtime_error("GetEnvironmentStrings() failed");
            }

            char* current = start;
            while (*current != '\0')
            {
                std::string_view s = current;
                auto pos = s.find("=");
                assert(pos != std::string_view::npos);
                std::string key = to_upper(s.substr(0, pos));
                if (!key.empty())
                {
                    std::string_view value = (pos != s.npos) ? s.substr(pos + 1) : "";
                    m[std::string(key)] = value;
                }
                current += s.size() + 1;
            }
#endif
            return m;
        }

        inline std::string platform()
        {
#ifndef _WIN32
            utsname un;
            int ret = uname(&un);

            if (ret == -1)
            {
                throw std::runtime_error("uname() failed");
            }

            return std::string(un.sysname);
#else
            return "win32";
#endif
        }

        inline fs::path home_directory()
        {
#ifdef _WIN32
            std::string maybe_home = env::get("USERPROFILE").value_or("");
            if (maybe_home.empty())
            {
                maybe_home
                    = concat(env::get("HOMEDRIVE").value_or(""), env::get("HOMEPATH").value_or(""));
            }
            if (maybe_home.empty())
            {
                throw std::runtime_error(
                    "Cannot determine HOME (checked USERPROFILE, HOMEDRIVE and HOMEPATH env vars)");
            }
#else
            std::string maybe_home = env::get("HOME").value_or("");
            if (maybe_home.empty())
            {
                maybe_home = getpwuid(getuid())->pw_dir;
            }
            if (maybe_home.empty())
            {
                throw std::runtime_error("HOME not set.");
            }
#endif
            return maybe_home;
        }

        inline fs::path expand_user(const fs::path& path)
        {
            auto p = path.string();
            if (p[0] == '~')
            {
                p.replace(0, 1, home_directory());
            }
            return p;
        }

        inline fs::path shrink_user(const fs::path& path)
        {
            auto p = path.string();
            auto home = home_directory().string();
            if (starts_with(p, home))
            {
                p.replace(0, home.size(), "~");
            }
            return p;
        }

        // inline fs::path expand_vars(const fs::path& path)
        // {
        //     #ifndef _WIN32
        //     wordexp_t w{};
        //     std::unique_ptr<wordexp_t, void(*)(wordexp_t*)> hold{&w, ::wordfree};
        //     ::wordexp(path.c_str(), &w, 0);
        //     if (w.we_wordc != 1)
        //         throw std::runtime_error("Cannot expand path: " + path.string());
        //     fs::path result = fs::absolute(w.we_wordv[0]);
        //     return result;
        //     #else
        //     // ExpandEnvironmentStringsW
        //     #endif
        // }
    }
}

#endif
