#include "cli/audit_command.h"
#include "backend/audit_component.h"
#include "common/utils.h"
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace sentio {
namespace cli {

int AuditCommand::execute(const std::vector<std::string>& args) {
    // Show help if requested
    if (has_flag(args, "--help") || has_flag(args, "-h")) {
        show_help();
        return 0;
    }
    
    // Parse subcommand (default to "summarize")
    std::string subcommand = "summarize";
    if (!args.empty() && args[0][0] != '-') {
        subcommand = args[0];
    }
    
    // Parse run ID
    std::string run_id = get_arg(args, "--run", "");
    run_id = resolve_run_id(run_id);
    
    if (run_id.empty()) {
        std::cerr << "ERROR: No trade books found in data/trades" << std::endl;
        return 3;
    }
    
    if (!validate_run_id(run_id)) {
        std::cerr << "ERROR: Trade book not found for run: " << run_id << std::endl;
        return 1;
    }
    
    // Execute subcommand
    if (subcommand == "summarize") {
        return execute_summarize(run_id);
    } else if (subcommand == "report" || subcommand == "position-history") {
        std::string max_str = get_arg(args, "--max", "20");
        size_t max_trades = static_cast<size_t>(std::stoul(max_str));
        return execute_report(run_id, max_trades);
    } else if (subcommand == "trade-list") {
        return execute_trade_list(run_id);
    } else {
        std::cerr << "Error: Unknown audit subcommand '" << subcommand << "'" << std::endl;
        show_help();
        return 1;
    }
}

void AuditCommand::show_help() const {
    std::cout << "Usage: sentio_cli audit [subcommand] [options]\n\n";
    std::cout << "Analyze trading performance with professional reports.\n\n";
    std::cout << "Subcommands:\n";
    std::cout << "  summarize          Show performance summary (default)\n";
    std::cout << "  report             Multi-section performance report with trade history\n";
    std::cout << "  trade-list         Complete list of all trades\n\n";
    std::cout << "Options:\n";
    std::cout << "  --run RUN_ID       Specific run ID to analyze (default: latest)\n";
    std::cout << "  --max N            Maximum trades to show in report (default: 20)\n";
    std::cout << "  --help, -h         Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sentio_cli audit\n";
    std::cout << "  sentio_cli audit report --max 50\n";
    std::cout << "  sentio_cli audit trade-list --run trade_20250922_143022\n";
}

int AuditCommand::execute_summarize(const std::string& run_id) {
    try {
        // For now, just show a message that this functionality needs to be implemented
        std::cout << "📊 Performance Summary for Run: " << run_id << std::endl;
        std::cout << "⚠️  Full summarize functionality will be implemented in next phase" << std::endl;
        std::cout << "📄 Trade book: data/trades/" << run_id << "_trades.jsonl" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Summarize failed: " << e.what() << std::endl;
        return 1;
    }
}

int AuditCommand::execute_report(const std::string& run_id, size_t max_trades) {
    try {
        std::cout << "📈 Position History Report for Run: " << run_id << std::endl;
        std::cout << "📊 Showing last " << max_trades << " trades" << std::endl;
        std::cout << "⚠️  Full report functionality will be implemented in next phase" << std::endl;
        std::cout << "📄 Trade book: data/trades/" << run_id << "_trades.jsonl" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Report generation failed: " << e.what() << std::endl;
        return 1;
    }
}

int AuditCommand::execute_trade_list(const std::string& run_id) {
    try {
        std::cout << "📋 Trade List for Run: " << run_id << std::endl;
        std::cout << "⚠️  Full trade list functionality will be implemented in next phase" << std::endl;
        std::cout << "📄 Trade book: data/trades/" << run_id << "_trades.jsonl" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Trade list generation failed: " << e.what() << std::endl;
        return 1;
    }
}

std::string AuditCommand::resolve_run_id(const std::string& run_input) {
    if (!run_input.empty() && run_input != "latest") {
        return run_input;
    }
    
    // Find latest trade file
    std::string trades_dir = "data/trades";
    if (!std::filesystem::exists(trades_dir)) {
        return "";
    }
    
    std::string latest_file;
    std::filesystem::file_time_type latest_time;
    
    for (const auto& entry : std::filesystem::directory_iterator(trades_dir)) {
        if (entry.is_regular_file() && 
            entry.path().filename().string().find("_trades.jsonl") != std::string::npos) {
            auto file_time = entry.last_write_time();
            if (latest_file.empty() || file_time > latest_time) {
                latest_file = entry.path().filename().string();
                latest_time = file_time;
            }
        }
    }
    
    if (!latest_file.empty()) {
        // Extract run ID from filename (format: <runid>_trades.jsonl)
        size_t pos = latest_file.find("_trades.jsonl");
        if (pos != std::string::npos) {
            return latest_file.substr(0, pos);
        }
    }
    
    return "";
}

bool AuditCommand::validate_run_id(const std::string& run_id) {
    std::string trade_book = "data/trades/" + run_id + "_trades.jsonl";
    return std::filesystem::exists(trade_book);
}

} // namespace cli
} // namespace sentio
