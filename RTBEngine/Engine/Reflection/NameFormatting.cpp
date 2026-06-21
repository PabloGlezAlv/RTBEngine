#include "NameFormatting.h"

#include <cctype>

namespace RTBEngine {
    namespace Reflection {

        namespace {

            bool ContainsSpace(const std::string& value)
            {
                return value.find(' ') != std::string::npos;
            }

            std::string InsertWordBoundaries(const std::string& identifier)
            {
                std::string spaced;
                spaced.reserve(identifier.size() + 8);

                for (size_t i = 0; i < identifier.size(); ++i) {
                    const char character = identifier[i];
                    if (character == '_') {
                        if (!spaced.empty() && spaced.back() != ' ') {
                            spaced += ' ';
                        }
                        continue;
                    }

                    if (i > 0) {
                        const char previous = identifier[i - 1];
                        const char next = (i + 1 < identifier.size()) ? identifier[i + 1] : '\0';

                        if (std::islower(static_cast<unsigned char>(previous)) &&
                            std::isupper(static_cast<unsigned char>(character))) {
                            spaced += ' ';
                        } else if (std::isupper(static_cast<unsigned char>(previous)) &&
                                   std::isupper(static_cast<unsigned char>(character)) &&
                                   std::islower(static_cast<unsigned char>(next))) {
                            spaced += ' ';
                        }
                    }

                    spaced += character;
                }

                return spaced;
            }

            std::string ToTitleCase(const std::string& spaced)
            {
                std::string result;
                result.reserve(spaced.size());

                bool newWord = true;
                for (const char character : spaced) {
                    if (character == ' ') {
                        if (!result.empty() && result.back() != ' ') {
                            result += ' ';
                        }
                        newWord = true;
                        continue;
                    }

                    if (newWord) {
                        result += static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
                        newWord = false;
                    } else {
                        result += character;
                    }
                }

                return result;
            }

        }

        std::string FormatPropertyName(const std::string& identifier)
        {
            if (identifier.empty() || ContainsSpace(identifier)) {
                return identifier;
            }

            return ToTitleCase(InsertWordBoundaries(identifier));
        }

        std::string FormatPropertyName(const char* identifier)
        {
            return identifier ? FormatPropertyName(std::string(identifier)) : std::string();
        }

    }
}
