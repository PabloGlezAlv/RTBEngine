#pragma once

#include "../RTBEngineAPI.h"
#include <string>

namespace RTBEngine {
    namespace Reflection {
        class TypeInfo;
    }

    namespace Data {

        class RTB_API DataAsset {
        public:
            virtual ~DataAsset() = default;

            virtual const char* GetTypeName() const = 0;
            virtual void* GetActualObject() { return this; }
            virtual const void* GetActualObject() const { return this; }
            virtual const Reflection::TypeInfo* GetTypeInfo() const;

            const std::string& GetSourcePath() const { return sourcePath; }
            void SetSourcePath(const std::string& path) { sourcePath = path; }

        private:
            std::string sourcePath;
        };

    }
}
