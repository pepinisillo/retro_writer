#pragma once

#include <functional>
#include <string>
#include <vector>

namespace rw_ia {

std::string jsonEscapeString(const std::string &s);

struct Config {
    /** POST application/json; respuesta JSON con "idea" o texto plano. Vacío = no se llama. */
    std::string endpoint;
    std::string endpoint2;
    std::string endpoint3;
    /** Opcional: POST para comentarios (cuerpo JSON libre desde la app). */
    std::string feedbackEndpoint;
    std::string feedbackEndpoint2;
    std::string openaiModel;
    /** Clave API o ruta a archivo de una sola línea con la clave. */
    std::string openaiApiKey;
    int timeoutSec {45};
};

bool loadConfig(const std::string &cfgPath, Config &out);

/**
 * Obtiene texto de idea.
 * Modos:
 * 0 frases_cortas, 1 diez_palabras, 2 cinco_palabras,
 * 3 palabra_japonesa, 4 tres_palabras_no_comunes, 5 tres_palabras_random.
 */
/**
 * Obtiene idea y, si el modelo/endpoint lo incluye, plazo en días (1..7).
 * Si no hay campo de días en la respuesta, deadlineDaysOut queda en 0 (el llamador puede usar randomDeadlineDays()).
 */
bool fetchIdea(const Config &c, int mode, std::string &ideaOut, int &deadlineDaysOut, std::string &errOut,
               const std::function<void(const std::string &)> &statusCb = {});

struct Assignment {
    std::string id;
    std::string novelRootAbs;
    std::string premisaAbs;
    std::string createdIso;
    std::string deadlineIso;
    /** pendiente | entregada | entregada_tarde */
    std::string estado;
    std::string modo;
};

bool appendJsonl(const std::string &path, const Assignment &a);
std::vector<Assignment> loadAll(const std::string &path);
std::vector<Assignment> loadPending(const std::string &path);
bool markDelivered(const std::string &path, const std::string &id, bool onTime);

/** Días 1..7 desde ahora (medianoche local aprox.). */
std::string deadlineIsoFromDays(int days);
int randomDeadlineDays();

/** POST JSON a `feedback_endpoint` de la config (comentario / revision). */
bool fetchFeedback(const Config &c, const std::string &bodyJson, std::string &replyOut, std::string &errOut,
                   const std::function<void(const std::string &)> &statusCb = {});

} // namespace rw_ia
