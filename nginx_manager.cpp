// nginx_manager.cpp
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>  // For geteuid
#include <cstdlib>   // For system, exit
#include <map>
#include <algorithm> // For std::all_of, std::transform
#include <cstring>   // <--- !!! تمت الإضافة هنا !!!
#include <limits>    // For std::numeric_limits

namespace fs = std::filesystem;

// ... (بقية الكود كما هو) ...

// --- Configuration Variables ---
std::string CONFIG_DIR_BASE = "/etc/nginx-manager";
std::string BACKUP_DIR_BASE = CONFIG_DIR_BASE + "/backups";
std::string CONFIG_FILE_PATH = CONFIG_DIR_BASE + "/nginx-manager.conf";

// --- Global Nginx and Tool Settings (loaded from config or defaults) ---
std::string nginx_prefix_path = "/etc/nginx";
std::string backup_on_edit_preference = ""; // "yes", "no", or "" (prompt)
std::string default_editor_preference = ""; // "nano", "vim", or "" (prompt)
std::string g_exit_after_action_preference = "no"; // Default, "yes" or "no"

// --- ANSI Color Codes ---
std::string GREEN = "\033[1;32m";
std::string RED = "\033[1;31m";
std::string CYAN = "\033[1;36m";
std::string BLUE = "\033[1;34m";
std::string YELLOW = "\033[1;33m";
std::string RESET = "\033[0m";

// --- Forward Declarations ---
void save_all_configs();
void load_all_configs();
bool handle_command_line_args(int argc, char* argv[]); // Returns true if program should exit
void print_help();

// --- Configuration Handling ---
void load_all_configs() {
    fs::create_directories(CONFIG_DIR_BASE);
    std::ifstream infile(CONFIG_FILE_PATH);
    if (infile) {
        std::string line;
        while (std::getline(infile, line)) {
            std::istringstream iss(line);
            std::string key, value;
            if (std::getline(iss, key, '=') && std::getline(iss, value)) {
                if (key == "prefix") {
                    nginx_prefix_path = value;
                } else if (key == "backup_on_edit") {
                    backup_on_edit_preference = value;
                } else if (key == "default_editor") {
                    default_editor_preference = value;
                } else if (key == "exit_after_action") {
                    g_exit_after_action_preference = value;
                }
            }
        }
    }
    fs::create_directories(BACKUP_DIR_BASE); // Ensure backup dir exists
}

void save_all_configs() {
    fs::create_directories(CONFIG_DIR_BASE);
    std::ofstream outfile(CONFIG_FILE_PATH);
    if (outfile) {
        outfile << "prefix=" << nginx_prefix_path << "\n";
        outfile << "backup_on_edit=" << backup_on_edit_preference << "\n";
        outfile << "default_editor=" << default_editor_preference << "\n";
        outfile << "exit_after_action=" << g_exit_after_action_preference << "\n";
    } else {
        std::cerr << RED << "Error: Could not write to config file: " << CONFIG_FILE_PATH << RESET << std::endl;
    }
}

bool is_valid_prefix(const std::string& prefix_to_check) {
    return fs::exists(prefix_to_check + "/nginx.conf") &&
           fs::is_directory(prefix_to_check + "/sites-available") &&
           fs::is_directory(prefix_to_check + "/sites-enabled");
}

// --- Utility Functions (most are the same as before) ---
bool is_symlink_active(const std::string& filename, const std::string& enabled_dir) {
    return fs::exists(enabled_dir + "/" + filename);
}

bool is_root() {
    return geteuid() == 0;
}

void reload_nginx() {
    std::cout << YELLOW << "Reloading Nginx..." << RESET << std::endl;
    int result = std::system("nginx -s reload");
    if (result == 0) {
        std::cout << GREEN << "Nginx reloaded successfully." << RESET << std::endl;
    } else {
        std::cout << RED << "Failed to reload Nginx. Check configuration (nginx -t)." << RESET << std::endl;
    }
}

void list_sites(const std::vector<std::string>& sites, const std::vector<bool>& states) {
    std::cout << YELLOW << "\n--- Available Sites ---\n" << RESET;
    for (size_t i = 0; i < sites.size(); ++i) {
        std::cout << i + 1 << ". " << CYAN << sites[i] << RESET << " ["
                  << (states[i] ? GREEN + "active" + RESET : RED + "off" + RESET) << "]\n";
    }
}

void list_backups() {
    std::vector<std::string> backups;
    if (!fs::exists(BACKUP_DIR_BASE) || !fs::is_directory(BACKUP_DIR_BASE)) {
        std::cout << RED << "Backup directory not found or is not a directory." << RESET << "\n";
        return;
    }
    for (const auto& entry : fs::directory_iterator(BACKUP_DIR_BASE)) {
        if(entry.is_regular_file()) { // Ensure we list only files
             backups.push_back(entry.path().filename().string());
        }
    }

    if (backups.empty()) {
        std::cout << RED << "No backups found." << RESET << "\n";
        return;
    }

    std::cout << YELLOW << "\n--- Available Backups ---\n" << RESET;
    for (size_t i = 0; i < backups.size(); ++i) {
        std::cout << i + 1 << ". " << BLUE << backups[i] << RESET << "\n";
    }
}

std::string generate_backup_filename(const std::string& base_filename) {
    std::string stem = fs::path(base_filename).stem().string();
    std::string ext = fs::path(base_filename).extension().string();
    // Try base_filename itself first if it doesn't look like a typical backup name pattern
    std::string fullpath_candidate = BACKUP_DIR_BASE + "/" + base_filename;
    
    // If base_filename doesn't exist, or if it exists but we want a unique _backupN name
    // This logic creates names like file.conf, file_backup.conf, file_backup1.conf, file_backup2.conf
    if (fs::exists(fullpath_candidate)) {
        fullpath_candidate = BACKUP_DIR_BASE + "/" + stem + "_backup" + ext;
    }
    
    int counter = 1;
    std::string current_candidate = fullpath_candidate;
    // If "file_backup.conf" exists, try "file_backup1.conf", etc.
    while (fs::exists(current_candidate)) {
        current_candidate = BACKUP_DIR_BASE + "/" + stem + "_backup" + std::to_string(counter) + ext;
        counter++;
    }
    return current_candidate;
}

void restore_backup(const std::string& backup_file_path, const std::string& target_file_path) {
    try {
        fs::copy_file(backup_file_path, target_file_path, fs::copy_options::overwrite_existing);
        std::cout << GREEN << "Backup restored successfully to: " << target_file_path << RESET << "\n";
    } catch (const fs::filesystem_error& e) {
        std::cerr << RED << "Failed to restore backup: " << e.what() << RESET << "\n";
    }
}

bool get_integer_input(int& number, int min_val = std::numeric_limits<int>::min(), int max_val = std::numeric_limits<int>::max()) {
    std::string line;
    std::getline(std::cin, line);
    line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
    line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);

    if (line.empty()) {
        std::cout << RED << "No input provided. Please enter a number." << RESET << std::endl;
        return false;
    }
    
    std::stringstream ss(line);
    if (ss >> number && ss.eof()) {
        if (number >= min_val && number <= max_val) {
            return true;
        } else {
             std::cout << RED << "Input " << number << " is out of range. Please enter a number between " << min_val << " and " << max_val << "." << RESET << std::endl;
             return false;
        }
    }
    std::cout << RED << "Invalid input '" << line << "'. Please enter a valid number." << RESET << std::endl;
    return false;
}


bool validate_multiple_numbers_input(const std::string& input_str_const, int min_val, int max_val, std::vector<int>& numbers) {
    std::string input_str = input_str_const;
    input_str.erase(0, input_str.find_first_not_of(" \t\n\r\f\v"));
    input_str.erase(input_str.find_last_not_of(" \t\n\r\f\v") + 1);

    std::istringstream iss(input_str);
    std::string token;
    numbers.clear();

    if (input_str.empty()) {
        std::cout << RED << "No input provided." << RESET << std::endl;
        return false;
    }

    while (iss >> token) {
        for (char const &c : token) {
            if (!std::isdigit(c)) {
                std::cout << RED << "Invalid character '" << c << "' in input token: '" << token << "'" << RESET << std::endl;
                return false;
            }
        }
        if (token.empty()) continue;

        try {
            int num = std::stoi(token);
            if (num < min_val || num > max_val) {
                std::cout << RED << "Number " << num << " is out of range (" << min_val << "-" << max_val << ")." << RESET << std::endl;
                return false;
            }
            numbers.push_back(num);
        } catch (const std::invalid_argument& ia) {
            std::cout << RED << "Invalid number format in token: '" << token << "'" << RESET << std::endl;
            return false;
        } catch (const std::out_of_range& oor) {
            std::cout << RED << "Number out of range in token: '" << token << "'" << RESET << std::endl;
            return false;
        }
    }
    if (numbers.empty() && !input_str.empty() && std::all_of(input_str.begin(), input_str.end(), ::isspace)) {
        std::cout << RED << "Input contained only spaces." << RESET << std::endl;
        return false;
    }
    if (numbers.empty() && !input_str.empty()){ // If not empty string but numbers is empty (e.g. "abc")
         std::cout << RED << "No valid numbers found in input: '" << input_str_const << "'" << RESET << std::endl;
        return false;
    }
    return !numbers.empty();
}


std::string ensure_conf_extension(const std::string& filename) {
    if (filename.length() < 5 || filename.substr(filename.length() - 5) != ".conf") {
        return filename + ".conf";
    }
    return filename;
}

void print_main_menu() {
    std::cout << BLUE << "\n--- NGINX Manager (Prefix: " << nginx_prefix_path << ") ---\n" << RESET
              << "1. Enable/Disable sites\n"
              << "2. Rename a site config\n"
              << "3. Edit a site config\n"
              << "4. Delete a site config\n"
              << "5. Edit main nginx.conf\n"
              << "7. Restore from backup\n"
              << "8. Delete backup\n"
              << "9. Nginx status/test/control (quick commands)\n"
              << "0. Exit\n"
              << YELLOW << "Current preferences: Backup on edit: "
              << (backup_on_edit_preference.empty() ? "Prompt" : backup_on_edit_preference)
              << ", Editor: " << (default_editor_preference.empty() ? "Prompt" : default_editor_preference)
              << ", Exit after action: " << g_exit_after_action_preference << RESET << "\n"
              << "Choice: ";
}

void handle_nginx_quick_commands() {
    std::cout << YELLOW << "\n--- Nginx Quick Commands ---\n" << RESET
              << "1. Start Nginx (systemctl start nginx)\n"
              << "2. Stop Nginx (systemctl stop nginx)\n"
              << "3. Restart Nginx (systemctl restart nginx)\n"
              << "4. Reload Nginx (nginx -s reload)\n"
              << "5. Test Configuration (nginx -t)\n"
              << "0. Back to main menu\n"
              << "Choice: ";
    int cmd_choice;
    if (!get_integer_input(cmd_choice, 0, 5)) return;

    std::string command_to_run;
    switch (cmd_choice) {
        case 1: command_to_run = "systemctl start nginx"; break;
        case 2: command_to_run = "systemctl stop nginx"; break;
        case 3: command_to_run = "systemctl restart nginx"; break;
        case 4: command_to_run = "nginx -s reload"; break;
        case 5: command_to_run = "nginx -t"; break;
        case 0: return;
        default: std::cout << RED << "Invalid choice." << RESET << std::endl; return;
    }
    if (!command_to_run.empty()) {
        std::cout << CYAN << "Executing: " << command_to_run << RESET << std::endl;
        std::system(command_to_run.c_str());
    }
}


// --- Main Application Logic ---
int main(int argc, char* argv[]) {
    if (!is_root()) {
        std::cerr << RED << "You must be root. Try using sudo." << RESET << std::endl;
        return 1;
    }

    load_all_configs();

    if (argc > 1) {
        if (handle_command_line_args(argc, argv)) {
            return 0;
        }
    }

    bool runtime_exit_after_action = (g_exit_after_action_preference == "yes");

    std::string sites_available_dir = nginx_prefix_path + "/sites-available/";
    std::string sites_enabled_dir = nginx_prefix_path + "/sites-enabled/";
    std::string nginx_conf_file = nginx_prefix_path + "/nginx.conf";

    if (!is_valid_prefix(nginx_prefix_path)) {
         std::cerr << RED << "Error: Invalid Nginx prefix path: '" << nginx_prefix_path << "'." << RESET << std::endl;
         std::cerr << RED << "Ensure nginx.conf, sites-available, and sites-enabled exist at this prefix." << RESET << std::endl;
         std::cerr << RED << "You can set a new prefix using: sudo ./nginx_manager --prefix /path/to/nginx" << RESET << std::endl;
         return 1;
    }


    while (true) {
        std::vector<std::string> sites;
        std::vector<bool> states;

        if (!fs::exists(sites_available_dir) || !fs::is_directory(sites_available_dir) ||
            !fs::exists(sites_enabled_dir) || !fs::is_directory(sites_enabled_dir)) {
            std::cerr << RED << "Error: Nginx 'sites-available' or 'sites-enabled' directory not found at current prefix: " << nginx_prefix_path << RESET << std::endl;
            std::cerr << RED << "Please check the Nginx installation or use '--prefix' to set the correct path and restart the tool." << RESET << std::endl;
            return 1;
        }

        for (const auto& entry : fs::directory_iterator(sites_available_dir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                sites.push_back(filename);
                states.push_back(is_symlink_active(filename, sites_enabled_dir));
            }
        }

        print_main_menu();

        int choice = -1;
        if (!get_integer_input(choice, 0, 9)) {
            continue;
        }

        bool action_taken_this_iteration = false;

        if (choice == 0) break;

        if (choice == 9) {
            handle_nginx_quick_commands();
        } else if (choice == 5) {
            std::string file_to_edit = nginx_conf_file;
            std::cout << "Editing main Nginx configuration: " << file_to_edit << std::endl;

            bool perform_backup = false;
            if (backup_on_edit_preference == "yes") {
                perform_backup = true;
            } else if (backup_on_edit_preference == "no") {
                perform_backup = false;
            } else {
                std::cout << "Backup current file before editing? (y/n, default n): ";
                std::string answer;
                std::getline(std::cin, answer);
                std::transform(answer.begin(), answer.end(), answer.begin(), ::tolower);
                if (answer == "y") {
                    perform_backup = true;
                    backup_on_edit_preference = "yes";
                } else {
                    backup_on_edit_preference = "no"; // Also save 'n' as a preference
                }
                save_all_configs();
            }

            if (perform_backup) {
                std::string backup_name = generate_backup_filename(fs::path(file_to_edit).filename().string());
                try {
                    fs::copy_file(file_to_edit, backup_name, fs::copy_options::overwrite_existing);
                    std::cout << GREEN << "Backup saved: " << backup_name << RESET << "\n";
                } catch (const fs::filesystem_error& e) {
                    std::cerr << RED << "Failed to create backup: " << e.what() << RESET << "\n";
                }
            }

            std::string editor_cmd;
            if (default_editor_preference == "nano" || default_editor_preference == "vim") {
                editor_cmd = default_editor_preference;
            } else {
                std::cout << "Choose editor: 1) nano, 2) vim (default nano): ";
                int editor_choice_num = 1;
                std::string editor_choice_str;
                std::getline(std::cin, editor_choice_str);
                if (!editor_choice_str.empty()) {
                    try { editor_choice_num = std::stoi(editor_choice_str); } catch (...) { /* use default */ }
                }
                editor_cmd = (editor_choice_num == 2) ? "vim" : "nano";
                default_editor_preference = editor_cmd;
                save_all_configs();
            }

            std::string cmd_str = editor_cmd + " " + file_to_edit;
            std::system(cmd_str.c_str());
            reload_nginx();
            action_taken_this_iteration = true;

        } else if (choice == 7) {
            std::vector<std::string> backups_list;
            if (fs::exists(BACKUP_DIR_BASE) && fs::is_directory(BACKUP_DIR_BASE)) {
                for (const auto& entry : fs::directory_iterator(BACKUP_DIR_BASE)) {
                    if(entry.is_regular_file()) backups_list.push_back(entry.path().filename().string());
                }
            }

            if (backups_list.empty()) {
                std::cout << RED << "No backups found." << RESET << "\n";
                continue;
            }

            list_backups();
            std::cout << "Enter backup number to restore or 0 to cancel: ";
            int num;
            if (!get_integer_input(num, 0, backups_list.size()) || num == 0) {
                continue;
            }

            std::string backup_filename_to_restore = backups_list[num - 1];
            std::string backup_full_path = BACKUP_DIR_BASE + "/" + backup_filename_to_restore;
            std::string target_file_path;

            if (backup_filename_to_restore.find("nginx.conf") != std::string::npos ||
                backup_filename_to_restore.find("nginx_backup") != std::string::npos) { // Heuristic
                target_file_path = nginx_conf_file;
            } else {
                std::string original_site_name = backup_filename_to_restore;
                size_t backup_suffix_pos = original_site_name.rfind("_backup");
                 if (backup_suffix_pos != std::string::npos) {
                    // !!! الخطأ كان هنا، تم استبدال strlen("_backup") بـ 7 !!!
                    std::string after_suffix_stem = fs::path(original_site_name.substr(backup_suffix_pos + 7)).stem().string();
                    bool looks_like_numbered_backup = true;
                    if (after_suffix_stem.empty() && fs::path(original_site_name.substr(backup_suffix_pos + 7)).extension().empty()) {
                        // This means it was like "file_backup.conf" -> after_suffix is ".conf", stem is ""
                        // This case is "file_backup.conf", not "file_backupN.conf"
                        // No further check needed for digits, it is a valid pattern
                    } else if (!after_suffix_stem.empty()) {
                        for(char c : after_suffix_stem) {
                            if (!isdigit(c)) {
                                looks_like_numbered_backup = false;
                                break;
                            }
                        }
                    } else { // after_suffix_stem is empty, but extension might not be, e.g. _backup.conf.bak
                        looks_like_numbered_backup = false; // or handle as desired
                    }

                    if(looks_like_numbered_backup){
                        original_site_name = original_site_name.substr(0, backup_suffix_pos) + fs::path(original_site_name).extension().string();
                    }
                }
                target_file_path = sites_available_dir + original_site_name;
                 std::cout << YELLOW << "Attempting to restore " << backup_filename_to_restore << " as " << CYAN << original_site_name << RESET << " in sites-available." << std::endl;
            }

            std::cout << "Restore backup " << CYAN << backup_filename_to_restore << RESET
                      << " to " << CYAN << target_file_path << RESET << "? (y/n): ";
            std::string confirm;
            std::getline(std::cin, confirm);
            std::transform(confirm.begin(), confirm.end(), confirm.begin(), ::tolower);
            if (confirm == "y") {
                restore_backup(backup_full_path, target_file_path);
                reload_nginx();
                action_taken_this_iteration = true;
            } else {
                std::cout << "Restore canceled." << std::endl;
            }
            continue;
        } else if (choice == 8) {
            std::vector<std::string> backups_list;
             if (fs::exists(BACKUP_DIR_BASE) && fs::is_directory(BACKUP_DIR_BASE)) {
                for (const auto& entry : fs::directory_iterator(BACKUP_DIR_BASE)) {
                     if(entry.is_regular_file()) backups_list.push_back(entry.path().filename().string());
                }
            }

            if (backups_list.empty()) {
                std::cout << RED << "No backups found to delete." << RESET << "\n";
                continue;
            }

            list_backups();
            std::cout << "Enter backup numbers to delete (e.g., 1 2 3) or 0 to cancel: ";
            std::string input_str;
            std::getline(std::cin, input_str);

            std::vector<int> numbers_to_delete;
            if (!validate_multiple_numbers_input(input_str, 0, backups_list.size(), numbers_to_delete)) {
                continue;
            }

            if (numbers_to_delete.empty() || (numbers_to_delete.size() == 1 && numbers_to_delete[0] == 0)) {
                std::cout << "Deletion canceled." << std::endl;
                continue;
            }
            
            std::sort(numbers_to_delete.rbegin(), numbers_to_delete.rend()); 
            numbers_to_delete.erase(std::unique(numbers_to_delete.begin(), numbers_to_delete.end()), numbers_to_delete.end());

            for (int num : numbers_to_delete) {
                if (num == 0) continue;
                if (num >= 1 && num <= static_cast<int>(backups_list.size())) {
                    std::string backup_to_delete_path = BACKUP_DIR_BASE + "/" + backups_list[num - 1];
                    try {
                        if (fs::remove(backup_to_delete_path)) {
                            std::cout << RED << "Deleted backup: " << backups_list[num - 1] << RESET << "\n";
                            action_taken_this_iteration = true;
                        } else {
                            std::cout << RED << "Failed to delete backup (it may not exist or permissions issue): " << backups_list[num - 1] << RESET << "\n";
                        }
                    } catch (const fs::filesystem_error& e) {
                        std::cerr << RED << "Error deleting backup " << backups_list[num - 1] << ": " << e.what() << RESET << "\n";
                    }
                } else {
                     std::cout << RED << "Invalid backup number for deletion: " << num << RESET << std::endl;
                }
            }

        } else if (choice >= 1 && choice <= 4) {
            if (sites.empty() && (choice >= 1 && choice <= 4) ) {
                std::cout << RED << "No site configuration files found in " << sites_available_dir << RESET << "\n";
                continue;
            }
            list_sites(sites, states);

            if (choice == 1) {
                std::cout << "Enter site numbers to toggle (e.g., 1 2 3) or 0 to cancel: ";
                std::string input_str;
                std::getline(std::cin, input_str);
                std::vector<int> site_nums;
                if (!validate_multiple_numbers_input(input_str, 0, sites.size(), site_nums)) {
                    continue;
                }
                if (site_nums.empty() || (site_nums.size() == 1 && site_nums[0] == 0)) continue;

                for (int num : site_nums) {
                    if (num < 1 || num > static_cast<int>(sites.size())) {
                         std::cout << RED << "Skipping invalid site number: " << num << RESET << std::endl;
                         continue;
                    }
                    std::string site_name = sites[num - 1];
                    std::string symlink_path = sites_enabled_dir + "/" + site_name;
                    std::string available_path = sites_available_dir + site_name;
                    if (states[num - 1]) {
                        try {
                            if(fs::exists(symlink_path)) {
                                if (fs::remove(symlink_path)) {
                                    std::cout << RED << "Disabled: " << site_name << RESET << "\n";
                                } else {
                                     std::cout << RED << "Failed to disable (could not remove symlink): " << site_name << RESET << "\n";
                                }
                            } else {
                                std::cout << YELLOW << "Symlink for " << site_name << " already removed or points elsewhere." << RESET << std::endl;
                            }
                        } catch (const fs::filesystem_error& e) {
                            std::cerr << RED << "Error disabling " << site_name << ": " << e.what() << RESET << "\n";
                        }
                    } else {
                        try {
                            fs::create_symlink(fs::absolute(available_path), symlink_path);
                            std::cout << GREEN << "Enabled: " << site_name << RESET << "\n";
                        } catch (const fs::filesystem_error& e) {
                             std::cerr << RED << "Error enabling " << site_name << ": " << e.what() << RESET << "\n";
                        }
                    }
                }
                reload_nginx();
                action_taken_this_iteration = true;

            } else if (choice == 2) {
                std::cout << "Enter site number to rename or 0 to cancel: ";
                int num;
                if (!get_integer_input(num, 0, sites.size()) || num == 0) continue;

                std::string old_name = sites[num - 1];
                bool was_active = states[num - 1];

                std::cout << "Enter new name for " << CYAN << old_name << RESET << " (e.g., mynewsite or mynewsite.conf): ";
                std::string new_name_raw;
                std::getline(std::cin, new_name_raw);
                if (new_name_raw.empty()) {
                    std::cout << RED << "New name cannot be empty. Operation canceled." << RESET << std::endl;
                    continue;
                }
                if (new_name_raw.find_first_of("/\\:*?\"<>|") != std::string::npos) {
                    std::cout << RED << "New name contains invalid characters. Operation canceled." << RESET << std::endl;
                    continue;
                }

                std::string new_name = ensure_conf_extension(new_name_raw);

                if (fs::exists(sites_available_dir + new_name)) {
                    std::cout << RED << "A site with the name '" << new_name << "' already exists. Operation canceled." << RESET << std::endl;
                    continue;
                }

                try {
                    fs::rename(sites_available_dir + old_name, sites_available_dir + new_name);
                    std::cout << GREEN << "Renamed in sites-available: " << old_name << " to " << new_name << RESET << "\n";
                    if (was_active) {
                        if(fs::exists(sites_enabled_dir + "/" + old_name)) fs::remove(sites_enabled_dir + "/" + old_name);
                        fs::create_symlink(fs::absolute(sites_available_dir + new_name), sites_enabled_dir + "/" + new_name);
                        std::cout << GREEN << "Updated symlink in sites-enabled." << RESET << "\n";
                    }
                    reload_nginx();
                    action_taken_this_iteration = true;
                } catch (const fs::filesystem_error& e) {
                    std::cerr << RED << "Error renaming site: " << e.what() << RESET << "\n";
                }


            } else if (choice == 3) {
                std::cout << "Enter site number to edit or 0 to cancel: ";
                int num;
                if (!get_integer_input(num, 0, sites.size()) || num == 0) continue;

                std::string file_to_edit = sites_available_dir + sites[num - 1];
                std::cout << "Editing site: " << sites[num-1] << std::endl;

                bool perform_backup = false;
                if (backup_on_edit_preference == "yes") {
                    perform_backup = true;
                } else if (backup_on_edit_preference == "no") {
                    perform_backup = false;
                } else {
                    std::cout << "Backup current file before editing? (y/n, default n): ";
                    std::string answer;
                    std::getline(std::cin, answer);
                    std::transform(answer.begin(), answer.end(), answer.begin(), ::tolower);
                    if (answer == "y") {
                        perform_backup = true;
                        backup_on_edit_preference = "yes";
                    } else {
                        backup_on_edit_preference = "no";
                    }
                    save_all_configs();
                }

                if (perform_backup) {
                    std::string backup_name = generate_backup_filename(sites[num - 1]);
                     try {
                        fs::copy_file(file_to_edit, backup_name, fs::copy_options::overwrite_existing);
                        std::cout << GREEN << "Backup saved: " << backup_name << RESET << "\n";
                    } catch (const fs::filesystem_error& e) {
                        std::cerr << RED << "Failed to create backup: " << e.what() << RESET << "\n";
                    }
                }

                std::string editor_cmd;
                 if (default_editor_preference == "nano" || default_editor_preference == "vim") {
                    editor_cmd = default_editor_preference;
                } else {
                    std::cout << "Choose editor: 1) nano, 2) vim (default nano): ";
                    int editor_choice_num = 1;
                    std::string editor_choice_str;
                    std::getline(std::cin, editor_choice_str);
                     if (!editor_choice_str.empty()) {
                        try { editor_choice_num = std::stoi(editor_choice_str); } catch (...) { /* use default */ }
                    }
                    editor_cmd = (editor_choice_num == 2) ? "vim" : "nano";
                    default_editor_preference = editor_cmd;
                    save_all_configs();
                }
                std::string cmd_str = editor_cmd + " " + file_to_edit;
                std::system(cmd_str.c_str());
                reload_nginx();
                action_taken_this_iteration = true;

            } else if (choice == 4) {
                std::cout << "Enter site numbers to delete (e.g. 1 2 3) or 0 to cancel: ";
                std::string input_str;
                std::getline(std::cin, input_str);
                std::vector<int> site_nums_to_delete;
                if (!validate_multiple_numbers_input(input_str, 0, sites.size(), site_nums_to_delete)) {
                    continue;
                }
                if (site_nums_to_delete.empty() || (site_nums_to_delete.size() == 1 && site_nums_to_delete[0] == 0)) {
                     std::cout << "Deletion canceled." << std::endl;
                     continue;
                }
                
                std::sort(site_nums_to_delete.rbegin(), site_nums_to_delete.rend());
                site_nums_to_delete.erase(std::unique(site_nums_to_delete.begin(), site_nums_to_delete.end()), site_nums_to_delete.end());

                for (int num : site_nums_to_delete) {
                     if (num < 1 || num > static_cast<int>(sites.size())) {
                         std::cout << RED << "Skipping invalid site number for deletion: " << num << RESET << std::endl;
                         continue;
                    }
                    std::string name = sites[num - 1];
                    std::cout << "Are you sure you want to delete " << RED << name << RESET << "? This is permanent. (y/n): ";
                    std::string confirm_delete;
                    std::getline(std::cin, confirm_delete);
                    std::transform(confirm_delete.begin(), confirm_delete.end(), confirm_delete.begin(), ::tolower);
                    if (confirm_delete == "y") {
                        try {
                            if(fs::exists(sites_available_dir + name)) fs::remove(sites_available_dir + name);
                            if (states[num - 1] && fs::exists(sites_enabled_dir + "/" + name)) {
                                fs::remove(sites_enabled_dir + "/" + name);
                            }
                            std::cout << RED << "Deleted: " << name << RESET << "\n";
                            action_taken_this_iteration = true;
                        } catch (const fs::filesystem_error& e) {
                            std::cerr << RED << "Error deleting site " << name << ": " << e.what() << RESET << "\n";
                        }
                    } else {
                        std::cout << "Deletion of " << name << " canceled." << std::endl;
                    }
                }
                if (action_taken_this_iteration) reload_nginx();
            }
        } else {
             if (choice != 0) {
                std::cout << RED << "Invalid choice. Please try again." << RESET << "\n";
            }
        }

        if (action_taken_this_iteration && runtime_exit_after_action) {
            std::cout << CYAN << "Exiting after action based on saved preference." << RESET << std::endl;
            break;
        }
    }

    std::cout << CYAN << "\nGoodbye." << RESET << std::endl;
    return 0;
}

void print_help() {
    std::cout << YELLOW << "Nginx Manager - A tool to manage Nginx configurations.\n\n"
              << "Usage: sudo ./nginx_manager [OPTIONS]\n\n"
              << "If no options are provided, the tool enters interactive mode.\n\n"
              << "OPTIONS:\n"
              << "  -h, --help                Show this help message and exit.\n\n"
              << "  Configuration Preferences (these are saved in " << CONFIG_FILE_PATH << " and tool exits after setting):\n"
              << "  --prefix <path>           Set the Nginx configuration prefix path (e.g., /etc/nginx).\n"
              << "                            Path must contain nginx.conf, sites-available/, and sites-enabled/.\n"
              << "  -b <yes|no>               Set preference for backing up config files before editing.\n"
              << "                            'yes': Always backup. 'no': Never backup.\n"
              << "                            If not set via flag, will ask on first edit in interactive mode.\n"
              << "  --editor <nano|vim>       Set the default text editor for configuration files.\n"
              << "                            If not set via flag, will ask on first edit in interactive mode.\n"
              << "  -e <yes|no>               Set preference to exit immediately after an action in interactive mode.\n"
              << "                            'yes': Exit after action. 'no': Return to menu.\n\n"
              << "  Nginx Direct Commands (these execute and then exit):\n"
              << "  -s start|stop|restart     Control Nginx service (uses systemctl).\n"
              << "                            Example: nginx_manager -s restart\n"
              << "  -t                        Test Nginx configuration (nginx -t).\n"
              << "  -r                        Reload Nginx configuration (nginx -s reload).\n"
              << RESET << std::endl;
}

bool handle_command_line_args(int argc, char* argv[]) {
    bool preference_updated = false;
    bool direct_command_executed = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_help();
            return true;
        } else if (arg == "--prefix") {
            if (i + 1 < argc) {
                std::string new_prefix = argv[++i];
                if (is_valid_prefix(new_prefix)) {
                    nginx_prefix_path = new_prefix;
                    std::cout << GREEN << "Nginx prefix set to: " << nginx_prefix_path << RESET << std::endl;
                    preference_updated = true;
                } else {
                    std::cerr << RED << "Invalid Nginx prefix path: " << new_prefix << std::endl;
                    std::cerr << "Path must contain nginx.conf, sites-available, and sites-enabled." << RESET << std::endl;
                    return true;
                }
            } else {
                std::cerr << RED << "--prefix option requires a path argument." << RESET << std::endl;
                return true;
            }
        } else if (arg == "-b") {
            if (i + 1 < argc) {
                std::string pref = argv[++i];
                std::transform(pref.begin(), pref.end(), pref.begin(), ::tolower);
                if (pref == "yes" || pref == "no") {
                    backup_on_edit_preference = pref;
                    std::cout << GREEN << "Backup preference set to: '" << backup_on_edit_preference << "'" << RESET << std::endl;
                    preference_updated = true;
                } else {
                    std::cerr << RED << "Invalid argument for -b. Use 'yes' or 'no'." << RESET << std::endl;
                    return true;
                }
            } else {
                std::cerr << RED << "-b option requires 'yes' or 'no'." << RESET << std::endl;
                return true;
            }
        } else if (arg == "--editor") {
            if (i + 1 < argc) {
                std::string editor_pref = argv[++i];
                std::transform(editor_pref.begin(), editor_pref.end(), editor_pref.begin(), ::tolower);
                if (editor_pref == "nano" || editor_pref == "vim") {
                    default_editor_preference = editor_pref;
                    std::cout << GREEN << "Default editor set to: '" << default_editor_preference << "'" << RESET << std::endl;
                    preference_updated = true;
                } else {
                    std::cerr << RED << "Invalid argument for --editor. Use 'nano' or 'vim'." << RESET << std::endl;
                    return true;
                }
            } else {
                std::cerr << RED << "--editor option requires 'nano' or 'vim'." << RESET << std::endl;
                return true;
            }
        } else if (arg == "-e") {
            if (i + 1 < argc) {
                std::string pref_val = argv[++i];
                std::transform(pref_val.begin(), pref_val.end(), pref_val.begin(), ::tolower);
                if (pref_val == "yes" || pref_val == "no") {
                    g_exit_after_action_preference = pref_val;
                    std::cout << GREEN << "Exit after action preference set to: '" << g_exit_after_action_preference << "'" << RESET << std::endl;
                    preference_updated = true;
                } else {
                    std::cerr << RED << "Invalid argument for -e. Use 'yes' or 'no'." << RESET << std::endl;
                    return true;
                }
            } else {
                std::cerr << RED << "-e option requires 'yes' or 'no'." << RESET << std::endl;
                return true;
            }
        } else if (arg == "-s") {
            if (i + 1 < argc) {
                std::string action = argv[++i];
                std::string cmd_str;
                if (action == "start") cmd_str = "systemctl start nginx";
                else if (action == "stop") cmd_str = "systemctl stop nginx";
                else if (action == "restart") cmd_str = "systemctl restart nginx";
                else {
                    std::cerr << RED << "Invalid action for -s. Use start, stop, or restart." << RESET << std::endl;
                    return true;
                }
                std::cout << CYAN << "Executing: " << cmd_str << RESET << std::endl;
                std::system(cmd_str.c_str());
                direct_command_executed = true;
            } else {
                std::cerr << RED << "-s option requires an action (start, stop, restart)." << RESET << std::endl;
                return true;
            }
        } else if (arg == "-t") {
            std::cout << CYAN << "Executing: nginx -t" << RESET << std::endl;
            std::system("nginx -t");
            direct_command_executed = true;
        } else if (arg == "-r") {
            std::cout << CYAN << "Executing: nginx -s reload" << RESET << std::endl;
            std::system("nginx -s reload");
            direct_command_executed = true;
        } else {
            std::cerr << RED << "Unknown option: " << arg << ". Use -h or --help for usage." << RESET << std::endl;
            return true;
        }
    }

    if (preference_updated) {
        save_all_configs();
        std::cout << GREEN << "Preferences saved to " << CONFIG_FILE_PATH << RESET << std::endl;
    }

    return direct_command_executed || preference_updated;
}

