#include "ia_writer.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace rw_ia {

std::string jsonEscapeString(const std::string &s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == '\n') {
            o += "\\n";
        } else if (c == '\r') {
        } else if (c == '"' || c == '\\') {
            o += '\\';
            o += char(c);
        } else {
            o += char(c);
        }
    }
    return o;
}

} // namespace rw_ia

namespace {

std::string trim(const std::string &s) {
    size_t i = 0;
    while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
    size_t j = s.size();
    while (j > i && std::isspace((unsigned char)s[j - 1])) j--;
    return s.substr(i, j - i);
}

std::string readFileTrim(const std::string &path) {
    std::ifstream in(path);
    if (!in) return {};
    std::string line;
    if (!std::getline(in, line)) return {};
    return trim(line);
}

static void appendIfNotEmptyUnique(std::vector<std::string> &v, const std::string &s) {
    const std::string t = trim(s);
    if (t.empty())
        return;
    for (const auto &x : v)
        if (x == t)
            return;
    v.push_back(t);
}

bool shellSingleQuoted(const std::string &path, std::string &out) {
    out.clear();
    out.push_back('\'');
    for (char c : path) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out.push_back('\'');
    return true;
}

bool runCurlToFile(const std::string &urlShell, const std::string &bodyFileShell, int timeoutSec, std::string &response,
                   std::string &err) {
    response.clear();
    err.clear();
    std::string cmd =
        "curl -sS -m " + std::to_string(std::max(5, timeoutSec)) + " -H 'Content-Type: application/json' "
        "--data-binary @" +
        bodyFileShell + " " + urlShell + " 2>&1";
    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp) {
        err = "popen curl fallo";
        return false;
    }
    char buf[4096];
    while (std::fgets(buf, sizeof buf, fp))
        response += buf;
    int st = pclose(fp);
    if (st != 0) {
        err = trim(response);
        if (err.empty())
            err = "curl codigo " + std::to_string(st);
        return false;
    }
    response = trim(response);
    return true;
}

std::string modeNameEs(int mode) {
    switch (mode) {
        case 0:
            return "frases_cortas";
        case 1:
            return "diez_palabras";
        case 2:
            return "cinco_palabras";
        case 3:
            return "palabra_japonesa";
        case 4:
            return "tres_palabras_no_comunes";
        case 5:
            return "tres_palabras_random";
        default:
            return "diez_palabras";
    }
}

std::string buildPromptEs(int mode) {
    switch (mode) {
        case 0:
            return "Dame una idea de escritura en una frase.";
        case 1:
            return "Dame una idea de escritura en 10 palabras.";
        case 2:
            return "Dame una idea de escritura en 5 palabras.";
        case 3:
            return "Dame solo una palabra japonesa.";
        case 4:
            return "Dame 3 palabras poco comunes en espanol que obligatoriamente deban aparecer en la historia.";
        case 5:
            return "Dame 3 palabras random que obligatoriamente deban aparecer en la historia.";
        default:
            return buildPromptEs(1);
    }
}


static bool extractJsonIntValueLoose(const std::string &j, const char *key, int &out) {
    /* Busca: "<key>" : 123  (con espacios opcionales) */
    const std::string kpat = std::string("\"") + key + "\"";
    size_t p = j.find(kpat);
    if (p == std::string::npos)
        return false;
    p += kpat.size();
    while (p < j.size() && std::isspace((unsigned char)j[p]))
        p++;
    if (p >= j.size() || j[p] != ':')
        return false;
    p++;
    while (p < j.size() && std::isspace((unsigned char)j[p]))
        p++;
    /* Soporta enteros o string numérico: "dias":3 o "dias":"3". */
    bool quoted = false;
    if (p < j.size() && j[p] == '"') {
        quoted = true;
        p++;
    }
    bool neg = false;
    if (p < j.size() && j[p] == '-') {
        neg = true;
        p++;
    }
    long val = 0;
    bool any = false;
    while (p < j.size() && std::isdigit((unsigned char)j[p])) {
        any = true;
        val = val * 10 + (j[p] - '0');
        if (val > 1000)
            break;
        p++;
    }
    if (quoted) {
        while (p < j.size() && std::isspace((unsigned char)j[p]))
            p++;
        if (p >= j.size() || j[p] != '"')
            return false;
    }
    if (!any)
        return false;
    out = int(neg ? -val : val);
    return true;
}

static bool extractJsonStringValueLoose(const std::string &j, const char *key, std::string &out) {
    /* Busca: "<key>" : "...."  (con espacios opcionales) */
    const std::string kpat = std::string("\"") + key + "\"";
    size_t p = j.find(kpat);
    if (p == std::string::npos)
        return false;
    p += kpat.size();
    while (p < j.size() && std::isspace((unsigned char)j[p]))
        p++;
    if (p >= j.size() || j[p] != ':')
        return false;
    p++;
    while (p < j.size() && std::isspace((unsigned char)j[p]))
        p++;
    if (p >= j.size() || j[p] != '"')
        return false;
    p++;
    out.clear();
    while (p < j.size()) {
        char c = j[p++];
        if (c == '\\') {
            if (p >= j.size()) break;
            char n = j[p++];
            if (n == 'n')
                out += '\n';
            else if (n == 't')
                out += '\t';
            else if (n == 'r') {
            } else
                out += n;
            continue;
        }
        if (c == '"')
            break;
        out += c;
    }
    return !out.empty();
}

static void applyDiasFromJson(const std::string &j, int &daysOut) {
    int d = 0;
    if (extractJsonIntValueLoose(j, "dias", d) || extractJsonIntValueLoose(j, "dias_disponibles", d)) {
        if (d >= 1)
            daysOut = std::clamp(d, 1, 7);
    }
}

/** Extrae texto del content del asistente (OpenAI chat completions y similares). */
static bool extractAssistantContentString(const std::string &body, std::string &out) {
    const char *needle = "\"content\":\"";
    size_t p = body.find(needle);
    if (p == std::string::npos)
        return false;
    p += std::strlen(needle);
    out.clear();
    while (p < body.size()) {
        char c = body[p++];
        if (c == '\\' && p < body.size()) {
            char n = body[p++];
            if (n == 'n')
                out += '\n';
            else
                out += n;
            continue;
        }
        if (c == '"')
            break;
        out += c;
    }
    return !out.empty();
}

bool parseIdeaAndDaysFromHttpBody(const std::string &body, std::string &idea, int &daysOut) {
    idea.clear();
    daysOut = 0;
    const std::string t = trim(body);
    if (t.empty())
        return false;

    if (extractJsonStringValueLoose(t, "idea", idea) && !idea.empty()) {
        applyDiasFromJson(t, daysOut);
        return true;
    }

    std::string innerFromMsg;
    if (extractAssistantContentString(t, innerFromMsg)) {
        std::string innerTrim = trim(innerFromMsg);
        if (innerTrim.size() >= 2 && innerTrim.front() == '{') {
            if (extractJsonStringValueLoose(innerTrim, "idea", idea) && !idea.empty()) {
                applyDiasFromJson(innerTrim, daysOut);
                return true;
            }
        }
        idea = innerTrim;
        return !idea.empty();
    }

    if (extractJsonStringValueLoose(t, "content", idea) && !idea.empty()) {
        std::string innerTrim = trim(idea);
        if (innerTrim.size() >= 2 && innerTrim.front() == '{' &&
            extractJsonStringValueLoose(innerTrim, "idea", idea) && !idea.empty()) {
            applyDiasFromJson(innerTrim, daysOut);
            return true;
        }
        return true;
    }

    if (t.find('{') == std::string::npos && t.find('[') == std::string::npos) {
        idea = t;
        return !idea.empty();
    }
    return false;
}

bool postOpenAI(const rw_ia::Config &c, const std::string &userContent, std::string &idea, int &daysOut, std::string &err) {
    if (c.openaiApiKey.empty()) {
        err = "sin clave OpenAI";
        return false;
    }
    const std::string model = c.openaiModel.empty() ? "gpt-4o-mini" : c.openaiModel;
    const std::string sys =
        "Responde SOLO con JSON en una sola linea, sin markdown ni texto extra.\n"
        "Formato exacto: {\"idea\":\"...\",\"dias\":N}\n"
        "idea: premisa breve en espanol.\n"
        "dias: entero de 1 a 7 (sin comillas) segun el plazo que tu recomiendes.";
    std::ostringstream json;
    json << "{\"model\":\"" << rw_ia::jsonEscapeString(model) << "\",\"messages\":["
         << "{\"role\":\"system\",\"content\":\"" << rw_ia::jsonEscapeString(sys) << "\"},"
         << "{\"role\":\"user\",\"content\":\"" << rw_ia::jsonEscapeString(userContent) << "\"}"
         << "]}";
    const unsigned long tag = (unsigned long)std::time(nullptr)
#if !defined(_WIN32)
                               ^ (unsigned long)getpid()
#endif
        ;
    std::string tmp = std::string("/tmp/rw_openai_") + std::to_string(tag) + ".json";
    {
        std::ofstream o(tmp, std::ios::trunc);
        if (!o) {
            err = "no se pudo escribir tmp";
            return false;
        }
        o << json.str();
    }
    std::string urlShell, fileShell;
    shellSingleQuoted("https://api.openai.com/v1/chat/completions", urlShell);
    shellSingleQuoted(tmp, fileShell);
    std::string authHdr = "Authorization: Bearer " + c.openaiApiKey;
    std::string keyFile = std::string("/tmp/rw_openai_hdr_") + std::to_string(tag) + ".txt";
    {
        std::ofstream hk(keyFile, std::ios::trunc);
        hk << authHdr;
    }
    std::string cmd = "curl -sS -m " + std::to_string(std::max(5, c.timeoutSec)) + " -H 'Content-Type: application/json' "
                      "-H @" +
                      keyFile + " --data-binary @" + fileShell + " " + urlShell + " 2>&1";
    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp) {
        std::remove(keyFile.c_str());
        std::remove(tmp.c_str());
        err = "popen curl openai";
        return false;
    }
    std::string response;
    char buf[4096];
    while (std::fgets(buf, sizeof buf, fp))
        response += buf;
    int st = pclose(fp);
    std::remove(keyFile.c_str());
    std::remove(tmp.c_str());
    response = trim(response);
    if (st != 0) {
        err = response.empty() ? "curl openai error" : response;
        return false;
    }
    /* La respuesta de OpenAI puede ser JSON (si sigues usando prompts anteriores) o texto plano.
       parseIdeaAndDaysFromHttpBody cubre ambos casos y, si no ve JSON, toma el cuerpo tal cual. */
    const bool ok = parseIdeaAndDaysFromHttpBody(response, idea, daysOut);
    if (!ok || idea.empty()) {
        /* Como ultimo recurso, usa el cuerpo crudo. */
        idea = response;
    }
    return !idea.empty();
}

} // namespace

namespace rw_ia {

bool loadConfig(const std::string &cfgPath, Config &out) {
    out = Config{};
    std::ifstream in(cfgPath);
    if (!in.is_open()) {
        const char *env = std::getenv("OPENAI_API_KEY");
        if (env && env[0])
            out.openaiApiKey = trim(std::string(env));
        return false;
    }
    std::string ver;
    if (!std::getline(in, ver)) return false;
    if (trim(ver) != "v1") return false;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        size_t sp = line.find(' ');
        if (sp == std::string::npos) continue;
        std::string key = trim(line.substr(0, sp));
        std::string val = trim(line.substr(sp + 1));
        if (key == "endpoint")
            out.endpoint = val;
        else if (key == "endpoint2")
            out.endpoint2 = val;
        else if (key == "endpoint3")
            out.endpoint3 = val;
        else if (key == "feedback_endpoint")
            out.feedbackEndpoint = val;
        else if (key == "feedback_endpoint2")
            out.feedbackEndpoint2 = val;
        else if (key == "openai_model")
            out.openaiModel = val;
        else if (key == "openai_key")
            out.openaiApiKey = val;
        else if (key == "openai_key_file")
            out.openaiApiKey = readFileTrim(val);
        else if (key == "timeout_sec")
            out.timeoutSec = std::max(5, std::atoi(val.c_str()));
    }
    const char *env = std::getenv("OPENAI_API_KEY");
    if (env && env[0] && out.openaiApiKey.empty())
        out.openaiApiKey = trim(std::string(env));
    return true;
}

bool fetchIdea(const Config &c, int mode, std::string &ideaOut, int &deadlineDaysOut, std::string &errOut,
               const std::function<void(const std::string &)> &statusCb) {
    ideaOut.clear();
    deadlineDaysOut = 0;
    errOut.clear();
    const std::string prompt = buildPromptEs(mode);
    const std::string modeStr = modeNameEs(mode);
    std::vector<std::string> endpoints;
    appendIfNotEmptyUnique(endpoints, c.endpoint);
    appendIfNotEmptyUnique(endpoints, c.endpoint2);
    appendIfNotEmptyUnique(endpoints, c.endpoint3);

    for (size_t i = 0; i < endpoints.size(); ++i) {
        const std::string &ep = endpoints[i];
        if (statusCb)
            statusCb("Enviando POST endpoint " + std::to_string(i + 1) + "...");
        std::ostringstream body;
        body << "{\"mode\":\"" << rw_ia::jsonEscapeString(modeStr) << "\",\"locale\":\"es\",\"prompt\":\""
             << rw_ia::jsonEscapeString(prompt) << "\"}";
        std::string tmp = std::string("/tmp/rw_ia_local_") + std::to_string((unsigned long)std::time(nullptr)) + ".json";
        {
            std::ofstream o(tmp, std::ios::trunc);
            if (!o) {
                errOut = "tmp json";
            } else {
                o << body.str();
            }
        }
        if (!errOut.empty()) {
            /* fall through */
        } else {
            std::string urlShell, fileShell;
            shellSingleQuoted(ep, urlShell);
            shellSingleQuoted(tmp, fileShell);
            std::string resp;
            if (statusCb)
                statusCb("Accediendo endpoint " + std::to_string(i + 1) + "...");
            if (runCurlToFile(urlShell, fileShell, c.timeoutSec, resp, errOut)) {
                std::remove(tmp.c_str());
                if (statusCb)
                    statusCb("Recibiendo respuesta endpoint " + std::to_string(i + 1) + "...");
                if (parseIdeaAndDaysFromHttpBody(resp, ideaOut, deadlineDaysOut))
                    return true;
                errOut = "respuesta sin campo idea/content";
            }
            std::remove(tmp.c_str());
        }
    }

    if (!c.openaiApiKey.empty()) {
        std::string e2;
        if (statusCb)
            statusCb("Accediendo OpenAI...");
        if (postOpenAI(c, prompt, ideaOut, deadlineDaysOut, e2) && !ideaOut.empty())
            return true;
        if (errOut.empty())
            errOut = e2;
        else
            errOut += " | " + e2;
    }

    (void)mode;
    if (errOut.empty())
        errOut = "No disponible: endpoint/OpenAI sin respuesta valida.";
    return false;
}

std::string deadlineIsoFromDays(int days) {
    days = std::clamp(days, 1, 7);
    using clock = std::chrono::system_clock;
    auto t = clock::now() + std::chrono::hours(24 * days);
    std::time_t tt = clock::to_time_t(t);
    struct tm tm {};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", &tm);
    return std::string(buf);
}

int randomDeadlineDays() {
    std::mt19937 rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
    return int(1 + (rng() % 7));
}

static std::string nowIso() {
    using clock = std::chrono::system_clock;
    std::time_t tt = clock::to_time_t(clock::now());
    struct tm tm {};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", &tm);
    return std::string(buf);
}

static std::string jsonLine(const Assignment &a) {
    std::ostringstream o;
    o << "{\"id\":\"" << jsonEscapeString(a.id) << "\",\"root\":\"" << jsonEscapeString(a.novelRootAbs)
      << "\",\"premisa\":\"" << jsonEscapeString(a.premisaAbs) << "\",\"created\":\"" << jsonEscapeString(a.createdIso)
      << "\",\"deadline\":\"" << jsonEscapeString(a.deadlineIso) << "\",\"estado\":\"" << jsonEscapeString(a.estado)
      << "\",\"modo\":\"" << jsonEscapeString(a.modo) << "\"}\n";
    return o.str();
}

bool appendJsonl(const std::string &path, const Assignment &a) {
    std::ofstream o(path, std::ios::app | std::ios::binary);
    if (!o) return false;
    o << jsonLine(a);
    return true;
}

static bool extractField(const std::string &line, const char *key, std::string &out) {
    const std::string pat = std::string("\"") + key + "\":\"";
    size_t p = line.find(pat);
    if (p == std::string::npos) return false;
    p += pat.size();
    out.clear();
    while (p < line.size()) {
        char c = line[p++];
        if (c == '\\') {
            if (p >= line.size()) break;
            out += line[p++];
            continue;
        }
        if (c == '"') break;
        out += c;
    }
    return true;
}

std::vector<Assignment> loadAll(const std::string &path) {
    std::vector<Assignment> v;
    std::ifstream in(path);
    if (!in) return v;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        Assignment a;
        if (!extractField(line, "id", a.id)) continue;
        extractField(line, "root", a.novelRootAbs);
        extractField(line, "premisa", a.premisaAbs);
        extractField(line, "created", a.createdIso);
        extractField(line, "deadline", a.deadlineIso);
        extractField(line, "estado", a.estado);
        extractField(line, "modo", a.modo);
        v.push_back(std::move(a));
    }
    return v;
}

std::vector<Assignment> loadPending(const std::string &path) {
    std::vector<Assignment> out;
    for (auto &a : loadAll(path)) {
        if (a.estado == "pendiente")
            out.push_back(std::move(a));
    }
    return out;
}

bool markDelivered(const std::string &path, const std::string &id, bool onTime) {
    auto all = loadAll(path);
    bool found = false;
    for (auto &a : all) {
        if (a.id == id) {
            a.estado = onTime ? "entregada" : "entregada_tarde";
            found = true;
            break;
        }
    }
    if (!found) return false;
    std::ofstream o(path, std::ios::trunc | std::ios::binary);
    if (!o) return false;
    for (const auto &a : all)
        o << jsonLine(a);
    return true;
}

bool fetchFeedback(const Config &c, const std::string &bodyJson, std::string &replyOut, std::string &errOut,
                   const std::function<void(const std::string &)> &statusCb) {
    replyOut.clear();
    errOut.clear();
    std::vector<std::string> endpoints;
    appendIfNotEmptyUnique(endpoints, c.feedbackEndpoint);
    appendIfNotEmptyUnique(endpoints, c.feedbackEndpoint2);
    if (endpoints.empty()) {
        errOut = "sin feedback_endpoint";
        return false;
    }
    std::string tmp = std::string("/tmp/rw_ia_fb_") + std::to_string((unsigned long)std::time(nullptr)) + ".json";
    {
        std::ofstream o(tmp, std::ios::trunc);
        if (!o) {
            errOut = "tmp";
            return false;
        }
        o << bodyJson;
    }
    bool ok = false;
    std::string fileShell;
    shellSingleQuoted(tmp, fileShell);
    for (size_t i = 0; i < endpoints.size(); ++i) {
        if (statusCb)
            statusCb("Enviando feedback endpoint " + std::to_string(i + 1) + "...");
        std::string urlShell;
        shellSingleQuoted(endpoints[i], urlShell);
        ok = runCurlToFile(urlShell, fileShell, c.timeoutSec, replyOut, errOut);
        if (ok)
            break;
    }
    std::remove(tmp.c_str());
    return ok;
}

} // namespace rw_ia
