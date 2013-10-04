#include <sys/wait.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <signal.h>
#define BUF_LEN 4096
typedef enum
{ false = 0, true = 1 } bool;
static bool childIsAlive = true;
static void statusChild (int sig)
{
  childIsAlive = false;
}

struct list
{
  char var[BUF_LEN];
  struct list *next;
};

void modifyPrompt (char *reply)
{
  reply[strlen (reply) - 6] = 0;
  write (STDOUT_FILENO, reply, strlen (reply));
  write (STDOUT_FILENO, "\n(gdb++) ", 9);
  memset (reply, 0, BUF_LEN);
}

void dialog (int from, int to, char *command, char *reply)
{
  char buf;
  write (to, command, strlen (command));
  memset (reply, 0, BUF_LEN);
  while (read (from, &buf, 1) > 0)
  {
    reply[strlen (reply)] = buf;
    if ((strlen (reply) > 5)
        && (strcmp (reply + strlen (reply) - 6, "(gdb) ") == 0))
      break;
  }
}

bool parseReply (char *reply)
{
  if (strstr (reply, "The program no longer exists.") != NULL)
    return false;
  return true;
}

int main (int argc, char *argv[])
{
  int par2ch[2], ch2par[2];
  pid_t cpid;
  char buf;
  char buffer[BUF_LEN], bufferUser[BUF_LEN], bufferPipe[BUF_LEN],
    reply[BUF_LEN];
  int retval, stepsCount, currentStep, i;
  fd_set rfds;
  struct list *variables = NULL;
  struct list *temp = NULL;
   /*STATEMENTS*/ bool readyToRecieve = false;
  bool sendCommand = false;
  bool pipeSentenseFinished = false;
  bool QUIT = false;
  /* END OF STATEMENTS */
  if (pipe (par2ch) == -1)
  {
    perror ("pipe par2ch");
    exit (EXIT_FAILURE);
  }
  if (pipe (ch2par) == -1)
  {
    perror ("pipe ch2par");
    exit (EXIT_FAILURE);
  }
  signal (SIGCHLD, statusChild);
  cpid = fork ();
  if (cpid == -1)
  {
    perror ("fork");
    exit (EXIT_FAILURE);
  }
  if (cpid == 0)
  {                             /* Child reads from pipe */
    close (par2ch[1]);          /* Close unused write end */
    close (ch2par[0]);
    dup2 (par2ch[0], STDIN_FILENO);
    dup2 (ch2par[1], STDOUT_FILENO);
    execv ("/usr/bin/gdb", argv);
    close (ch2par[1]);
    close (par2ch[0]);
    _exit (EXIT_SUCCESS);
  }
  else
  {                             /* Parent */
    close (par2ch[0]);
    close (ch2par[1]);
    memset (buffer, 0, BUF_LEN);
    while (read (ch2par[0], &buf, 1) > 0)
    {
      buffer[strlen (buffer)] = buf;
      if ((strlen (buffer) > 5)
          && (strcmp (buffer + strlen (buffer) - 6, "(gdb) ") == 0))
        break;
    }
    buffer[strlen (buffer) - 6] = 0;
    write (STDOUT_FILENO, buffer, strlen (buffer));
    write (STDOUT_FILENO, "\n", 1);
    write (STDOUT_FILENO, "(gdb++) ", 8);
    while (childIsAlive)
    {
      readyToRecieve = true;
      while (1)
      {
        if (readyToRecieve)
        {
          /*Recieve block */
          FD_ZERO (&rfds);
          FD_SET (STDIN_FILENO, &rfds);
          FD_SET (ch2par[0], &rfds);
          retval = select (ch2par[0] + 1, &rfds, NULL, NULL, NULL);
          if (retval > -1)
            if (FD_ISSET (STDIN_FILENO, &rfds))
            {
              /* Recieve 1 byte from user */
              if (read (STDIN_FILENO, &buf, 1) > 0)
                bufferUser[strlen (bufferUser)] = buf;

              if (strcmp (bufferUser, "quit\n") == 0)
              {
                QUIT = true;
                readyToRecieve = false;
                sendCommand = false;
                pipeSentenseFinished = false;
                continue;
              }
              if (bufferUser[strlen (bufferUser) - 1] == '\n')
              {
                readyToRecieve = false;
                sendCommand = true;
                continue;
              }
            }
          if (FD_ISSET (ch2par[0], &rfds))
          {
            /* Recieve 1 byte from pipe */
            if (read (ch2par[0], &buf, 1) > 0)
              bufferPipe[strlen (bufferPipe)] = buf;
            if ((strlen (bufferPipe) > 5)
                && (strcmp (bufferPipe + strlen (bufferPipe) - 6, "(gdb) ") ==
                    0))
            {
              readyToRecieve = false;
              pipeSentenseFinished = true;
              continue;
            }
          }
        }
        if (sendCommand)
        {
          if (strncmp (bufferUser, "run", 3) == 0)
          {
            variables = malloc (sizeof (struct list));
            memset (reply, 0, BUF_LEN);
            readyToRecieve = false;
            sendCommand = false;
            dialog (ch2par[0], par2ch[1], bufferUser, reply);
            strcpy (variables->var, reply);
            variables->next = NULL;
            stepsCount = 1;
            currentStep = 1;
            modifyPrompt (reply);
            readyToRecieve = true;
            memset (bufferUser, 0, BUF_LEN);
            continue;
          }
          else
            if (strcmp (bufferUser, "next\n") == 0
                || strcmp (bufferUser, "n\n") == 0)
          {
            if (currentStep >= stepsCount)
            {
              readyToRecieve = false;
              memset (reply, 0, BUF_LEN);
              dialog (ch2par[0], par2ch[1], "n\n", reply);
              temp = malloc (sizeof (struct list));
              strcpy (temp->var, reply);
              temp->next = variables;
              variables = temp;
              stepsCount++;
              currentStep++;
              modifyPrompt (reply);
              sendCommand = false;
              readyToRecieve = true;
              memset (bufferUser, 0, BUF_LEN);
              continue;
            }
            else
            {
              readyToRecieve = false;
              currentStep++;
              temp = variables;
              for (i = 1; i < stepsCount - currentStep + 1; i++)
                temp = temp->next;
              readyToRecieve = false;
              memset (bufferPipe, 0, BUF_LEN);
              strcpy (bufferPipe, temp->var);
              sprintf (bufferPipe, "%s", bufferPipe);
              sendCommand = false;
              pipeSentenseFinished = true;
              memset (bufferUser, 0, BUF_LEN);
              continue;
            }
          }
          else if (strcmp (bufferUser, "back\n") == 0)
          {
            readyToRecieve = false;
            if (currentStep > 1)
            {
              currentStep--;
              temp = variables;
              for (i = 1; i < stepsCount - currentStep + 1; i++)
                temp = temp->next;
              readyToRecieve = false;
              memset (bufferPipe, 0, BUF_LEN);
              strcpy (bufferPipe, temp->var);
              sprintf (bufferPipe, "%s", bufferPipe);
            }
            else
            {
              memset (bufferPipe, 0, BUF_LEN);
              sprintf (bufferPipe, "Already at the begin\n(gdb) ");
            }
            sendCommand = false;
            pipeSentenseFinished = true;
            memset (bufferUser, 0, BUF_LEN);
            continue;
          }
          else if (strcmp (bufferUser, "show position\n") == 0)
          {
            readyToRecieve = false;
            memset (bufferPipe, 0, BUF_LEN);
            sprintf (bufferPipe, "Position: [%d/%d]\n(gdb) ", currentStep,
                     stepsCount);
            sendCommand = false;
            pipeSentenseFinished = true;
            memset (bufferUser, 0, BUF_LEN);
            continue;
          }
          else if (strcmp (bufferUser, "snapshots\n") == 0)
          {
            readyToRecieve = false;
            do
            {
              memset (reply, 0, BUF_LEN);
              dialog (ch2par[0], par2ch[1], "n\n", reply);
              temp = malloc (sizeof (struct list));
              strcpy (temp->var, reply);
              temp->next = variables;
              variables = temp;
              stepsCount++;
            }
            while (parseReply (reply));
            memset (bufferPipe, 0, BUF_LEN);
            sprintf (bufferPipe,
                     "\n***\nProgram was failed for %d steps.\n(gdb) ",
                     stepsCount);
            sendCommand = false;
            pipeSentenseFinished = true;
            memset (bufferUser, 0, BUF_LEN);
            continue;
          }
          else if (strncmp (bufferUser, "gotostep ", 9) == 0)
          {
            currentStep = atoi (bufferUser + 9);
            if (currentStep <= stepsCount)
            {
              temp = variables;
              for (i = 1; i < stepsCount - currentStep + 1; i++)
                temp = temp->next;
              readyToRecieve = false;
              memset (bufferPipe, 0, BUF_LEN);
              strcpy (bufferPipe, temp->var);
              sprintf (bufferPipe, "%s", bufferPipe);
            }
            else
            {
              printf ("\nPlease input correct number of step\n(gdb++) ");
            }
            sendCommand = false;
            pipeSentenseFinished = true;
            memset (bufferUser, 0, BUF_LEN);
            continue;
          }
          else
          {
            write (par2ch[1], bufferUser, strlen (bufferUser));
            readyToRecieve = true;
            sendCommand = false;
            memset (bufferUser, 0, BUF_LEN);
            continue;
          }
        }
        if (pipeSentenseFinished)
        {
          bufferPipe[strlen (bufferPipe) - 6] = 0;
          write (STDOUT_FILENO, bufferPipe, strlen (bufferPipe));
          write (STDOUT_FILENO, "\n(gdb++) ", 9);
          readyToRecieve = true;
          pipeSentenseFinished = false;
          memset (bufferPipe, 0, BUF_LEN);
          continue;
        }
        if (QUIT)
        {
          write (par2ch[1], "quit\n", 5);
          sleep (1);
          break;
        }
      }
    }
    close (ch2par[0]);
    exit (EXIT_SUCCESS);
  }
}
