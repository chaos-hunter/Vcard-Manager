#!/usr/bin/env python3

from asciimatics.widgets import Frame, ListBox, Layout, Divider, Text, \
    Button, TextBox, Widget, PopUpDialog
from asciimatics.scene import Scene
from asciimatics.screen import Screen
from asciimatics.exceptions import ResizeScreenError, NextScene, StopApplication
import sys
import sqlite3
import os
import mysql.connector
import datetime
from ctypes import *
#c to python handling
library = "./libvcparser.so"
functions = CDLL(library)
functions.createCard.argtypes = [c_char_p, POINTER(c_void_p)]
functions.createCard.restype = c_int
functions.validateCard.argtypes = [c_void_p]
functions.validateCard.restype = c_int
functions.alternate.argtypes = []
functions.alternate.restype = c_void_p
functions.setfn.argtypes = [c_void_p, c_char_p]
functions.setfn.restype = c_int
functions.writeCard.argtypes = [c_char_p, c_void_p]
functions.writeCard.restype  = c_int
functions.getfn.argtypes = [c_void_p]
functions.getfn.restype = c_char_p
functions.getbday.argtypes = [c_void_p]
functions.getbday.restype  = c_char_p
functions.getanniv.argtypes = [c_void_p]
functions.getanniv.restype = c_char_p



class Login(Frame):
    def __init__(self, screen, model):
        super(Login, self).__init__(screen,
                                        screen.height * 5 // 6,
                                        screen.width * 5 // 6,
                                        hover_focus=True,
                                        can_scroll=False,
                                        title="Login")
        self._model = model
        layout = Layout([1], fill_frame=True)
        self.add_layout(layout)
        self._username = Text(label="Username:", name="username")
        self._password = Text(label="Password:", name="password")
        self._dbname = Text(label="DB name:", name="databasename")
        layout.add_widget(self._username)
        layout.add_widget(self._password)
        layout.add_widget(self._dbname)
        layout2 = Layout([1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("Login", self._login), 0)
        layout2.add_widget(Button("Cancel", self._quit), 1)
        self.fix()

    def _login(self):
        self.save()
        username = self.data["username"]
        password = self.data["password"]
        dbname = self.data["databasename"]
        self._model.username = username
        self._model.password = password
        self._model.dbname = dbname

        try:
            # Try to create the ContactModel and populate the database.
            self._model.contact_model = ContactModel(username, password, dbname)
            self._model.contact_model.populate_database()
            message = "Login was successful"
            # On success, transition to the main view.
            self._scene.add_effect(
                PopUpDialog(
                    self.screen,
                    message,
                    ["OK"],
                    on_close=lambda selected: self._main()))
        except mysql.connector.Error as err:
            message = "Login was not successful: " + str(err) + "\nPlease re-enter your credentials."
            self._scene.add_effect(
                PopUpDialog(
                    self.screen,
                    message,
                    ["OK"],
                    on_close=lambda selected: None))

    @staticmethod
    def _quit():
        raise StopApplication("User pressed quit")

    def _main(self):
        raise NextScene("Main")


class Model:
    def __init__(self):
        self.username = None
        self.password = None
        self.dbname = None
        self.contact_model = None

class ContactModel():
    def __init__(self, username, password, dbname):
        self.conn = mysql.connector.connect(
            host="dursley.socs.uoguelph.ca",
            user=username,
            password=password,
            database=dbname
        )
        
        self.conn.autocommit = True
        self.cursor = self.conn.cursor()
        # Create the FILE table with the specified schema.
        self.cursor.execute("""
            CREATE TABLE IF NOT EXISTS FILE (
                file_id INT AUTO_INCREMENT PRIMARY KEY,
                file_name VARCHAR(60) NOT NULL,
                last_modified DATETIME,
                creation_time DATETIME NOT NULL
            )
        """)
        # Create the CONTACT table with the specified schema.
        self.cursor.execute("""
            CREATE TABLE IF NOT EXISTS CONTACT (
                contact_id INT AUTO_INCREMENT PRIMARY KEY,
                name VARCHAR(256) NOT NULL,
                birthday DATETIME,
                anniversary DATETIME,
                file_id INT NOT NULL,
                FOREIGN KEY (file_id) REFERENCES FILE(file_id) ON DELETE CASCADE
            )
        """)

    def populate_database(self):
        folder = "cards"
        self.cursor.execute("DELETE FROM CONTACT")
        self.cursor.execute("DELETE FROM FILE")
        
        for fname in get_valid_vcard_files():
            full_path = os.path.join(folder, fname)
            last_modified = datetime.datetime.fromtimestamp(os.path.getmtime(full_path))
            creation_time = datetime.datetime.now()

            point = c_void_p()
            functions.createCard(full_path.encode(), byref(point))
            name = functions.getfn(point).decode()
            birthday_str = functions.getbday(point).decode()
            anniv_str = functions.getanniv(point).decode()
            
            # Insert into the FILE table.
            self.cursor.execute(
                "INSERT INTO FILE(file_name, last_modified, creation_time) VALUES(%s, %s, %s)",
                (fname, last_modified, creation_time)
            )
            file_id = self.cursor.lastrowid
            
            # Insert into the CONTACT table.
            self.cursor.execute(
                "INSERT INTO CONTACT(name, birthday, anniversary, file_id) VALUES(%s, %s, %s, %s)",
                (name, birthday_str if birthday_str else None, anniv_str if anniv_str else None, file_id)
            )
            

    def get_summary(self):
        self.cursor.execute("SELECT file_name FROM FILE ORDER BY file_id")
        return [(row[0], row[0]) for row in self.cursor.fetchall()]

    def close(self):
        self.cursor.close()
        self.conn.close()


def create(filename: str, contactname: str, model: Model):
    path = os.path.join("cards", filename)
    if not filename.endswith(".vcf"):
        return "Incorrect file"
    if os.path.exists(path):
        return "That file already exists"

    card = functions.alternate()
    if functions.setfn(card, contactname.encode()) != 0:
        return "Invalid contact name"

    if functions.validateCard(card) != 0:
        return "Validation failed"

    func = functions.writeCard(path.encode(), card)
    name = functions.getfn(card).decode()
    bday = functions.getbday(card).decode()
    anniv = functions.getanniv(card).decode()

    cursor = model.contact_model.cursor
    now = datetime.datetime.now()

    cursor.execute(
        "INSERT INTO FILE(file_name, last_modified, creation_time) VALUES (%s, %s, %s)",
        (filename, now, now)
    )
    file_id = cursor.lastrowid

    cursor.execute(
        "INSERT INTO CONTACT(name, birthday, anniversary, file_id) VALUES (%s, %s, %s, %s)",
        (name, bday if bday else None, anniv if anniv else None, file_id)
    )

    return 0
def get_valid_vcard_files():
    pointer = c_void_p()
    folder = "cards"
    files = []
    for filename in os.listdir(folder):
        if filename.endswith(".vcf"):
            directory = os.path.join(folder, filename)
            func = functions.createCard(directory.encode("utf-8"), byref(pointer))
            if functions.validateCard(pointer) == 0:
                files.append(filename)
            
    return files

class ListView(Frame):
    def __init__(self, screen, model):
        super(ListView, self).__init__(screen,
                                       screen.height * 5 // 6,
                                       screen.width * 5 // 6,
                                       on_load=self._reload_list,
                                       hover_focus=True,
                                       can_scroll=False,
                                       title="Vcard List")
        # Save off the model that accesses the contacts database.
        self._model = model
        files = (
            model.contact_model.get_summary() 
            if model.contact_model is not None 
            else [(fname, fname) for fname in get_valid_vcard_files()]
        )

        self._list_view = ListBox(
        Widget.FILL_FRAME,
        files,
        model.contact_model.get_summary() if model.contact_model else [],
        name="cards",
        add_scroll_bar=True,
        on_select=self._edit
        )
        self._edit_button = Button("Edit", self._edit)
        self._db_query = Button("DB queries", self._db_query)

        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        layout.add_widget(self._list_view)
        layout.add_widget(Divider())

        layout2 = Layout([1, 1, 1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("Create", self._add), 0)
        layout2.add_widget(self._edit_button, 1)
        layout2.add_widget(self._db_query, 2)
        layout2.add_widget(Button("Quit", self._quit), 3)

        self.fix()
    
    def _reload_list(self, new_value=None):
        if self._model.contact_model is not None:
            self._list_view.options = self._model.contact_model.get_summary()
        else:
            self._list_view.options = [(fname, fname) for fname in get_valid_vcard_files()]
        self._list_view.value = new_value

    def _add(self):
        raise NextScene("Create vCard")

    def _edit(self):
        self.save()
        self._model.current = self.data["cards"]
        raise NextScene("Edit vCard")

    def _db_query(self):
        raise NextScene("DB Query")
    
    @staticmethod
    def _quit():
        raise StopApplication("User pressed quit")


class CreateCardView(Frame):
    def __init__(self, screen, model):
        super().__init__(screen, 
                        screen.height*5//6, 
                        screen.width*5//6,
                        hover_focus=True, 
                        can_scroll=False, 
                        title="Create vCard")
        self._model = model
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        layout.add_widget(Text("File name:", "filename"))
        layout.add_widget(Text("Contact:", "fn"))
        layout2 = Layout([1,1,1,1])
        self.add_layout(layout2)
        layout2.add_widget(Button("OK", self._ok), 0)
        layout2.add_widget(Button("Cancel", self._cancel), 3)
        self.fix()

    def reset(self):
        super().reset()
        self.data = {"filename":"", "fn":""}

    def _ok(self):
        self.save()
        result = create(self.data["filename"], self.data["fn"], self._model)   
        if result !=0 :
            self._scene.add_effect(PopUpDialog(self.screen, result, ["OK"]))
            return
        raise NextScene("Main")

    def _cancel(self):
        raise NextScene("Main")



class EditCardView(Frame):
    def __init__(self, screen, model):
        super().__init__(screen,
                         screen.height*5//6,
                         screen.width*5//6,
                         hover_focus=True,
                         can_scroll=False,
                         title="vCard Details")
        self._model = model                  

        form = Layout([100], fill_frame=True)
        self.add_layout(form)
        form.add_widget(Text("File name:", "filename", readonly=True))
        form.add_widget(Text("Contact:", "fn"))
        form.add_widget(Text("Birthday:", "bday", readonly=True))
        form.add_widget(Text("Anniversary:", "anniv", readonly=True))
        form.add_widget(Text("Other properties:", "props", readonly=True))
        layout2 = Layout([1,1,1,1])
        self.add_layout(layout2)
        layout2.add_widget(Button("OK", self._ok), 0)
        layout2.add_widget(Button("Cancel", self._cancel), 3)
        self.fix()
    
    def _edit(self):
        self.save()
        if not self.data["cards"]:
            self._scene.add_effect(
                PopUpDialog(
                    self.screen,
                    "No file selected.",
                    ["OK"]
                )
            )
            return
        self._model.current = self.data["cards"]
        raise NextScene("Edit vCard")

    def reset(self):
        pointer = c_void_p()
        super().reset()
        fname = self._model.current
        if fname is None:
            # Optionally, you could set default values or return to the main view.
            self.data = {"filename": "", "fn": "", "bday": "", "anniv": ""}
            return
        functions.createCard(os.path.join("cards", fname).encode(), byref(pointer))
        self._model.current_card = pointer
        self.data = {
            "filename": fname,
            "fn": functions.getfn(pointer).decode(),
            "bday": functions.getbday(pointer).decode(),
            "anniv": functions.getanniv(pointer).decode(),
        }



    def _ok(self):
        self.save()
        card = self._model.current_card
        check = [
        (functions.setfn, (card, self.data["fn"].encode())),
        (functions.validateCard, (card,)),
        (functions.writeCard, (os.path.join("cards", self.data["filename"]).encode(), card))
        ]

        if any(fn(*args) != 0 for fn, args in check):
            dialog(self.screen, "Invalid update", ["OK"])
            return
        self._model.contact_model.cursor.execute("UPDATE CONTACT SET name=%s WHERE file_id = (SELECT file_id FROM FILE WHERE file_name=%s)",
        (self.data["fn"], self.data["filename"]))

        raise NextScene("Main")

    def _cancel(self):
        raise NextScene("Main")

class DBquery(Frame):
    def __init__(self, screen, model):
        super().__init__(screen, 
                         screen.height*5//6, 
                         screen.width*5//6,
                         hover_focus=True, 
                         can_scroll=False, 
                         title="DB Queries")
        self._model = model  
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        self._output = TextBox(
            Widget.FILL_FRAME,
            
            name="output",
            readonly=True,
            as_string=True,
            line_wrap=True
        )
        layout.add_widget(self._output)

        layout2 = Layout([1,1,1])
        self.add_layout(layout2)
        layout2.add_widget(Button("Display all", self._show_all), 0)
        layout2.add_widget(Button("Find contacts in June", self._find_june), 1)
        layout2.add_widget(Button("Cancel", self._cancel), 2)

        self.fix()

    def reset(self):
        super().reset()
        self.data = {"output": ""}

    def _show_all(self):
        cursor = self._model.contact_model.cursor
        cursor.execute("""
            SELECT CONCAT(FILE.file_name, ' : ', CONTACT.name)
            FROM CONTACT
            JOIN FILE ON CONTACT.file_id = FILE.file_id
        """)
        text = "\n".join(f[0] for f in cursor.fetchall())
        self.data["output"] = text
        self._output.value = text
        self._output.update(self.screen)


    def _find_june(self):
        cursor = self._model.contact_model.cursor
        cursor.execute("""
        SELECT CONTACT.name, CONTACT.birthday
        FROM CONTACT
        WHERE MONTH(CONTACT.birthday) = 6
        ORDER BY CONTACT.birthday ASC
        """)

        row = cursor.fetchall()
        text = "\n".join(f"{name} | {bday.strftime('%Y-%m-%d')}" for name, bday in row)
        self.data["output"] = text
        self._output.value = text
        self._output.update(self.screen)

    def _cancel(self):
        raise NextScene("Main")


def demo(screen, scene):
    scenes = [
        Scene([Login(screen, model)], -1, name="Login"),
        Scene([ListView(screen, model)], -1, name="Main"),
        Scene([CreateCardView(screen, model)], -1, name="Create vCard"),
        Scene([EditCardView(screen, model)], -1, name="Edit vCard"),
        Scene([DBquery(screen, model)], -1, name="DB Query")
        ]
    screen.play(scenes, stop_on_resize=True, start_scene=scene, allow_int=True)

model = Model()

last_scene = None
while True:
    try:
        Screen.wrapper(demo, catch_interrupt=True, arguments=[last_scene])
        sys.exit(0)
    except ResizeScreenError as e:
        last_scene = e.scene
    except StopApplication:
        if model.contact_model:
            cursor = model.contact_model.cursor
            cursor.execute("DELETE FROM CONTACT")
            cursor.execute("DELETE FROM FILE")
            model.contact_model.conn.commit()
            mode.contact_model.close
    break