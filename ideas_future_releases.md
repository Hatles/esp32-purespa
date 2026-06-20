# Suggestions pour les futures versions de l'administration PureSpa

Ce document regroupe les idées d'améliorations avancées pour le module d'administration du contrôleur PureSpa.

## 1. Mise à jour du firmware par navigateur (OTA - Over-The-Air)
- **Objectif** : Permettre de mettre à jour le firmware C++ de l'ESP32 directement depuis l'interface Web (en choisissant un fichier binaire `.bin`), sans avoir à le brancher en USB à un ordinateur.
- **Détails techniques** :
  - Utilisation de la bibliothèque `esp_ota_ops.h` d'ESP-IDF.
  - Endpoint `POST /api/admin/ota` traitant les données multipart et écrivant les blocs dans la partition OTA inactive avant de redémarrer sur la nouvelle partition.

## 2. Console de Logs de débogage en direct
- **Objectif** : Diagnostiquer les dysfonctionnements du Spa à distance sans moniteur série USB.
- **Détails techniques** :
  - Rediriger les sorties standards d'ESP_LOG dans un buffer circulaire en mémoire vive (RAM).
  - Fournir un endpoint `GET /api/admin/logs` pour envoyer les logs récents sous forme de texte ou de flux SSE vers l'interface d'administration.

## 3. Verrouillage par code PIN
- **Objectif** : Protéger l'accès au panneau d'administration pour éviter les modifications ou réinitialisations accidentelles par des enfants ou des invités.
- **Détails techniques** :
  - Configuration d'un code PIN simple (ex: 4 chiffres) stocké en NVS.
  - Écran intermédiaire bloquant dans la web UI demandant le code PIN avec un clavier numérique de style iOS.

## 4. Export / Import de configuration (Sauvegarde)
- **Objectif** : Télécharger une sauvegarde de tous les événements programmés au format JSON, et pouvoir les réimporter après une réinitialisation d'usine.
- **Détails techniques** :
  - Côté Web : Générer un fichier `.json` à partir de la réponse de `/api/schedule` et fournir un bouton d'import de fichier.
  - Côté API : Endpoint `POST /api/schedule/import` remplaçant le planning actuel en NVS par les données fournies.
