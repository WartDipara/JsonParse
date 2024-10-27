#include <variant>
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <optional>
#include <regex>
#include <charconv>
#include "Print.hpp"

struct JsonObject;

using JsonDiction = std::unordered_map<std::string, JsonObject>;
using JsonList = std::vector<JsonObject>;

struct JsonObject
{
    std::variant<
        std::nullptr_t, // null
        bool,           // true
        int,            // 123
        double,         // 1.123
        std::string,    //"hello"
        JsonDiction,    //{"hello": 123, "world": 456}
        JsonList>       //[123,"hello"]
        inner;

    void do_print() const
    {
        print(inner);
    }
    template <class T>
    bool is() const
    {
        return std::holds_alternative<T>(inner);
    }

    template <class T>
    T const &get() const
    {
        return std::get<T>(inner);
    }

    template <class T>
    T &get()
    {
        return std::get<T>(inner);
    }
};
template <class T>
std::optional<T> try_parse_num(std::string_view str)
{
    T value;
    auto res = std::from_chars(str.data(), str.data() + str.size(), value);
    if (res.ec == std::errc() && res.ptr == str.data() + str.size())
        return value;
    return std::nullopt;
}

char unescaped_char(char c)
{
    switch (c)
    {
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case '0':
        return '\0';
    case 't':
        return '\t';
    case 'v':
        return '\v';
    case 'f':
        return '\f';
    case 'b':
        return '\b';
    case 'a':
        return '\a';
    default:
        return c;
    }
}

std::pair<JsonObject, size_t> parse(std::string_view json)
{
    if (json.empty())
    {
        return {JsonObject{std::nullptr_t{}}, 0};
    }
    else if (size_t off = json.find_first_not_of(" \n\t\r\v\f\0"); off != 0 && off != json.npos)
    {
        auto [obj, eaten] = parse(json.substr(off));
        return {std::move(obj), eaten + off};
    }
    else if ('0' <= json[0] && json[0] <= '9' || json[0] == '+' || json[0] == '-')
    {
        std::regex num_re{"[+-]?[0-9]+(\\.[0-9]*)?([eE][+-]?[0-9]+)?"};
        std::cmatch match;
        if (std::regex_search(json.data(), json.data() + json.size(), match, num_re))
        {
            std::string str = match.str();
            if (auto num = try_parse_num<int>(str))
            {
                return {JsonObject{*num}, str.size()};
            }
            if (auto num = try_parse_num<double>(str))
            {
                return {JsonObject{*num}, str.size()};
            }
        }
    }
    else if (json[0] == '"')
    {
        std::string str;
        enum
        {
            Raw,
            Escaped
        } phase = Raw;
        size_t i;
        for (i = 1; i < json.size(); i++)
        {
            char ch = json[i];
            if (phase == Raw)
            {
                if (ch == '\\')
                {
                    phase = Escaped;
                }
                else if (ch == '"')
                {
                    i += 1;
                    break;
                }
                else
                {
                    str += ch;
                }
            }
            else if (phase == Escaped)
            {
                str += unescaped_char(ch);
                phase = Raw;
            }
        }
        return {JsonObject{std::move(str)}, i};
    }
    else if (json[0] == '[')
    {
        std::vector<JsonObject> res;
        size_t i;
        for (i = 1; i < json.size();)
        {
            if (json[i] == ']')
            {
                i += 1;
                break;
            }
            auto [obj, eaten] = parse(json.substr(i));
            if (eaten == 0)
            {
                i = 0;
                break;
            }
            res.push_back(std::move(obj));
            i += eaten;
            if (json[i] == ',')
            {
                i += 1;
            }
        }
        return {JsonObject{std::move(res)}, i};
    }
    else if (json[0] == '{')
    {
        std::unordered_map<std::string, JsonObject> res;
        size_t i;
        for (i = 1; i < json.size();)
        {
            if (json[i] == '}')
            {
                i += 1;
                break;
            }
            auto [keyObj, keyEaten] = parse(json.substr(i));
            if (keyEaten == 0)
            {
                i = 0;
                break;
            }
            i += keyEaten;
            if (!std::holds_alternative<std::string>(keyObj.inner))
            {
                i = 0;
                break;
            }
            if (json[i] == ':')
            {
                i += 1;
            }
            std::string key = std::move(std::get<std::string>(keyObj.inner));
            auto [valObj, valEaten] = parse(json.substr(i));
            if (valEaten == 0)
            {
                i = 0;
                break;
            }
            i += valEaten;
            res.try_emplace(std::move(key), std::move(valObj));
            if (json[i] == ',')
                i += 1;
        }
        return {JsonObject{std::move(res)}, i};
    }else if(json[0]=='\''){
        std::string str;
        enum
        {
            Raw,
            Escaped
        } phase = Raw;
        size_t i;
        for (i = 1; i < json.size(); i++)
        {
            char ch = json[i];
            if (phase == Raw)
            {
                if (ch == '\\')
                {
                    phase = Escaped;
                }
                else if (ch == '\'')
                {
                    i += 1;
                    break;
                }
                else
                {
                    str += ch;
                }
            }
            else if (phase == Escaped)
            {
                str += unescaped_char(ch);
                phase = Raw;
            }
        }
        return {JsonObject{std::move(str)}, i};
    }
    return {JsonObject{std::nullptr_t{}}, 0};
}

//hard to extends
// struct Functor
// {
//     void operator()(int val) const
//     {
//         print("int is:", val);
//     }
//     void operator()(double val) const
//     {
//         print("double is:", val);
//     }
//     void operator()(std::string val) const
//     {
//         print("string is:", val);
//     }
//     template <class T>
//     void operator()(T val) const
//     {
//         print("unknown object is:", val);
//     }
// };


template<class... Fs>
struct overloaded : Fs...{
    using Fs::operator()...;
};
template<class... Fs>
overloaded(Fs...)-> overloaded<Fs...>;


int main()
{
    // std::string_view str = R"JSON({
    // "work": 996,
    // "school": [985,211,[422,855]]
    // })JSON";

    // auto [obj, eaten] = parse(str);
    // std::cout << "obj is int? ";
    // print(obj.is<int>());
    // std::cout << "obj is double? ";
    // print(obj.is<double>());
    // std::cout << "obj is JsonDirect? ";
    // print(obj.is<JsonDiction>());
    // std::cout << "obj is JsonList? ";
    // print(obj.is<JsonList>());

    // auto const &dict = obj.get<JsonDiction>();
    // print("The capitalist make me work in", dict.at("work").get<int>());

    // auto const &school = dict.at("school");
    // auto reflect
    // lambda recurse !!!
    // must declare type , e.g. ->void
    // must send himself be a param
    //  auto doVisit = [&](auto& doVisit,JsonObject const& school)->void{
    //      std::visit(
    //          [&](auto const& school){
    //              if constexpr (std::is_same_v<std::decay_t<decltype(school)>,JsonList>){
    //                  for(auto const& s:school){
    //                      doVisit(doVisit,s);
    //                  }
    //              }else{
    //                  print("I purchased my diploma from",school);
    //              }
    //          }
    //      ,school.inner);
    //  };
    //  doVisit(doVisit,school);


    //hard to extends 
    // std::string_view str2 = R"JSON(
    // 985
    // )JSON";
    // std::string_view str3 = R"JSON(
    // "hello"
    // )JSON";
    // std::string_view str4 = R"JSON(
    // 4.16
    // )JSON";
    // auto [obj, eaten] = parse(str2);
    // auto [obj1, eaten1] = parse(str3);
    // auto [obj2, eaten2] = parse(str4);
    // print(obj);
    // std::visit(Functor(), obj.inner);
    // std::cout << std::endl;
    // print(obj1);
    // std::visit(Functor(), obj1.inner);
    // std::cout << std::endl;
    // print(obj2);
    // std::visit(Functor(), obj2.inner);

    std::string_view str = R"JSON('test')JSON";
    auto [obj, eaten] = parse(str);
    print(obj);
    std::visit(
        overloaded{
            [&] (int val) {
                print("int is:", val);
            },
            [&] (double val) {
                print("double is:", val);
            },
            [&] (std::string val) {
                print("string is:", val);
            },
            [&] (auto val) {
                print("unknown object is:", val);
            },
        },
        obj.inner);

    return 0;
}