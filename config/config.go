package config

import (
    "errors"
    "os"
    "os/exec"
    "path/filepath"
    "strings"
)


func getConfigFile() (string, error) {
    home, err := os.UserHomeDir()                                                                                                                                                            
    if err != nil {                                                                                                                                                                          
        return "", err                                                                                                                                                                       
    }                                                                                                                                                                                        
                                                                                                                                                                                             
    configFile := filepath.Join(home, ".config/megatron/config.txt")                                                                                                                         
    if _, err = os.Stat(configFile); errors.Is(err, os.ErrNotExist) {                                                                                                                        
        return "", err                                                                                                                                                                       
    }  

    return configFile, err
}

func GetRootDir() (string, error) {
    configFile, err := getConfigFile()
     if err != nil {
        return "", err
    }
  

    content, err := os.ReadFile(configFile)
    if err != nil {
        return "", err
    }


    rootDir := strings.Trim(string(content), " \n")

    fileInfo, err := os.Stat(rootDir)
    if err != nil {
    	return "", err
    }
    
    if !fileInfo.IsDir() {
    	return "", errors.New("path is not a directory")
    } 

    return rootDir, nil
}


func EditConfig() error {
    configFile, err := getConfigFile()
     if err != nil {
        return err
    }

    editor := os.Getenv("EDITOR")
    if editor == "" {
        editor = "vi"
    }

    cmd := exec.Command(editor, configFile)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
}
